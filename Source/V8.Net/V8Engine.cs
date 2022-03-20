/* All V8.NET source is governed by the LGPL licensing model. Please keep these comments intact, thanks.
 * Developer: James Wilkins (jameswilkins.net).
 * Source, Documentation, and Support: https://v8dotnet.codeplex.com
 */

using System;
using System.Runtime.CompilerServices;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Permissions;
using System.Text;
using System.Threading;
using System.Web;

namespace V8.Net
{
    // ========================================================================================================================
    // (.NET and Mono Marshalling: http://www.mono-project.com/Interop_with_Native_Libraries)
    // (Mono portable code: http://www.mono-project.com/Guidelines:Application_Portability)
    // (Cross platform p/invoke: http://www.gordonml.co.uk/software-development/mono-net-cross-platform-dynamic-pinvoke/)

    /// <summary>
    /// Creates a new managed V8Engine wrapper instance and associates it with a new native V8 engine.
    /// The engine does not implement locks, so to make it thread safe, you should lock against an engine instance (i.e. lock(myEngine){...}).  The native V8
    /// environment, however, is thread safe (but blocks to allow only one thread at a time).
    /// </summary>
    public unsafe partial class V8Engine : IDisposable
    {
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Set to the fixed date of Jan 1, 1970. This is used when converting DateTime values to JavaScript Date objects.
        /// </summary>
        public static readonly DateTime Epoch = new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);

        // --------------------------------------------------------------------------------------------------------------------

        public static string Version
        {
            get
            {
                return _Version ?? (_Version = FileVersionInfo.GetVersionInfo(typeof(V8Engine).Assembly.Location).FileVersion);
            }
        }
        static string _Version;

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// A static array of all V8 engines created.
        /// </summary>
        public V8Engine[] Engines { get { return _Engines; } }
        internal static V8Engine[] _Engines = new V8Engine[10];

        void _RegisterEngine(int engineID) // ('engineID' is the managed side engine ID, which starts at 0)
        {
            lock (_Engines)
            {
                if (engineID >= _Engines.Length)
                {
                    Array.Resize(ref _Engines, (5 + engineID) * 2); // (if many engines get allocated, for whatever crazy reason, "*2" creates an exponential capacity increase)
                }
                _Engines[engineID] = this;
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

        internal NativeV8EngineProxy* _NativeV8EngineProxy;
        V8GarbageCollectionRequestCallback ___V8GarbageCollectionRequestCallback; // (need to keep a reference to this delegate object, otherwise the reverse p/invoke will fail)
        static readonly object _GlobalLock = new object();

        ObjectTemplate _GlobalObjectTemplateProxy;

        internal Context _Context;

        internal NativeContext* _NativeContext;

        public InternalHandle GlobalObject { get { return _GlobalObject; } }
        internal InternalHandle _GlobalObject; // TODO: Consider supporting a new global context.

#if !(V1_1 || V2 || V3 || V3_5)
        public dynamic DynamicGlobalObject { get { return GlobalObject; } }
#endif

        // --------------------------------------------------------------------------------------------------------------------

        ///// <summary>
        ///// The sub-folder that is the root for the dependent libraries (x86 and x64).  This is set to "V8.NET" by default.
        ///// <para>This setting allows copying the V8.NET libraries to a project, and having the assemblies "Copy if never" automatically.</para>
        ///// </summary>
        //public static string AlternateRootSubPath = "V8.NET";

        //static Exception _TryLoadProxyInterface(string assemblyRoot, Exception lastError, out Assembly assembly)
        //{
        //    assembly = null;

        //    //// ... validate access to the root folder ...
        //    //var permission = new FileIOPermission(FileIOPermissionAccess.Read, assemblyRoot);
        //    //var permissionSet = new PermissionSet(PermissionState.None);
        //    //permissionSet.AddPermission(permission);
        //    //if (!permissionSet.IsSubsetOf(AppDomain.CurrentDomain.PermissionSet))

        //    if (!Directory.Exists(assemblyRoot))
        //        return new DirectoryNotFoundException("The path '" + assemblyRoot + "' does not exist, or is not accessible.");

        //    // ... check for a "V8.NET" sub-folder, which allows for copying the assemblies to a child folder of the project ...
        //    // (DLLs in the "x86" and "x64" folders of this child folder can be set to "Copy if newer" using this method)

        //    if (Directory.Exists(Path.Combine(assemblyRoot, AlternateRootSubPath)))
        //        assemblyRoot = Path.Combine(assemblyRoot, AlternateRootSubPath); // (exists, so move into it as the new root)

        //    // ... get platform details ...

        //    var bitStr = IntPtr.Size == 8 ? "x64" : "x86";
        //    var platformLibraryPath = Path.Combine(assemblyRoot, bitStr);
        //    string fileName;

        //    // ... if the platform folder doesn't exist, try loading assemblies from the current folder ...

        //    if (Directory.Exists(platformLibraryPath))
        //        fileName = Path.Combine(platformLibraryPath, "V8.Net.Proxy.Interface." + bitStr + ".dll");
        //    else
        //        fileName = Path.Combine(assemblyRoot, "V8.Net.Proxy.Interface." + bitStr + ".dll");

        //    // ... attempt to update environment variable automatically for the native DLLs ...
        //    // (see: http://stackoverflow.com/questions/7996263/how-do-i-get-iis-to-load-a-native-dll-referenced-by-my-wcf-service
        //    //   and http://stackoverflow.com/questions/344608/unmanaged-dlls-fail-to-load-on-asp-net-server)

        //    try
        //    {
        //        // ... add the search location to the path so "Assembly.LoadFrom()" can find other dependant assemblies if needed ...

        //        var path = System.Environment.GetEnvironmentVariable("PATH"); // TODO: Detect other systems if necessary.
        //        System.Environment.SetEnvironmentVariable("PATH", platformLibraryPath + ";" + path);
        //    }
        //    catch { }

        //    //// ... check 
        //    //var bitLibFolder = Path.Combine(Directory.GetCurrentDirectory(), bitStr);
        //    //if (Directory.Exists(bitLibFolder))
        //    //    Directory.SetCurrentDirectory(bitLibFolder);

        //    try
        //    {
        //        assembly = Assembly.LoadFrom(fileName);
        //    }
        //    catch (Exception ex)
        //    {
        //        var msg = "Failed to load '" + fileName + "': " + ex.GetFullErrorMessage();
        //        if (lastError != null)
        //            return new FileNotFoundException(msg + Environment.NewLine + Environment.NewLine, lastError);
        //        else
        //            return new FileNotFoundException(msg + Environment.NewLine + Environment.NewLine);
        //    }

        //    return null;
        //}

        //static Assembly Resolver(object sender, ResolveEventArgs args)
        //{
        //    if (args.Name.StartsWith("V8.Net.Proxy.Interface"))
        //    {
        //        AppDomain.CurrentDomain.AssemblyResolve -= Resolver;  // (handler is only needed once)

        //        Exception error = null;
        //        string assemblyRoot = "";
        //        Assembly assembly = null;

        //        // ... first check for a bin folder for ASP.NET sites ...

        //        if (HttpContext.Current != null)
        //        {
        //            assemblyRoot = HttpContext.Current.Server.MapPath("~/bin");
        //            if (Directory.Exists(assemblyRoot))
        //            {
        //                error = _TryLoadProxyInterface(assemblyRoot, error, out assembly);
        //                if (assembly != null) return assembly;
        //            }
        //        }

        //        // ... next check 'codebaseuri' - this is the *original* assembly location before it was cached for ASP.NET pages ...

        //        var codebaseuri = Assembly.GetExecutingAssembly().CodeBase;
        //        Uri codebaseURI = null;
        //        if (Uri.TryCreate(codebaseuri, UriKind.Absolute, out codebaseURI))
        //        {
        //            assemblyRoot = Path.GetDirectoryName(codebaseURI.LocalPath); // (check pre-shadow copy location first)
        //            error = _TryLoadProxyInterface(assemblyRoot, error, out assembly);
        //            if (assembly != null) return assembly;
        //        }

        //        // ... not found, so try the executing assembly's own location ...
        //        // (note: this is not done first, as the executing location might be in a cache location and not the original location!!!)

        //        var thisAssmeblyLocation = Assembly.GetExecutingAssembly().Location;
        //        if (!string.IsNullOrEmpty(thisAssmeblyLocation))
        //        {
        //            assemblyRoot = Path.GetDirectoryName(thisAssmeblyLocation); // (check loaded assembly location next)
        //            error = _TryLoadProxyInterface(assemblyRoot, error, out assembly);
        //            if (assembly != null) return assembly;
        //        }

        //        // ... finally, try the current directory ...

        //        assemblyRoot = Directory.GetCurrentDirectory(); // (check loaded assembly location next)
        //        error = _TryLoadProxyInterface(assemblyRoot, error, out assembly);
        //        if (assembly != null) return assembly;

        //        // ... if the current directory has an x86 or x64 folder for the bit depth, automatically change to that folder ...
        //        // (this is required to load the correct VC++ libraries if made available locally)

        //        var bitStr = IntPtr.Size == 8 ? "x64" : "x86";
        //        var msg = "Failed to load 'V8.Net.Proxy.Interface.x??.dll'.  V8.NET is running in the '" + bitStr + "' mode.  Some areas to check: " + Environment.NewLine
        //            + "1. The VC++ 2012 redistributable libraries are included, but if missing  for some reason, download and install from the Microsoft Site." + Environment.NewLine
        //            + "2. Did you download the DLLs from a ZIP file? If done so on Windows, you must open the file properties of the zip file and 'Unblock' it before extracting the files." + Environment.NewLine;

        //        if (HttpContext.Current != null)
        //            msg += "3. Make sure the path '" + assemblyRoot + "' is accessible to the application pool identity (usually Read & Execute for 'IIS_IUSRS', or a similar user/group)" + Environment.NewLine;
        //        else
        //            msg += "3. Make sure the path '" + assemblyRoot + "' is accessible to the application for loading the required libraries." + Environment.NewLine;

        //        if (error != null)
        //            throw new InvalidOperationException(msg + Environment.NewLine, error);
        //        else
        //            throw new InvalidOperationException(msg + Environment.NewLine);
        //    }

        //    return null;
        //}

        //static V8Engine()
        //{
        //    AppDomain.CurrentDomain.AssemblyResolve += Resolver;
        //}

        /// <summary> V8Engine constructor. </summary>
        /// <param name="autoCreateGlobalContext">
        ///     (Optional) True to automatically create a global context. If this is false then you must construct a context
        ///     yourself before executing JavaScript or calling methods that require contexts.
        /// </param>
        public V8Engine(bool autoCreateGlobalContext = true, IJsConverter jsConverter = null)
        {
            RunMarshallingTests();

            lock (_GlobalLock) // (required because engine proxy instance IDs are tracked on the native side in a static '_DisposedEngines' vector [for quick disposal of handles])
            {
                _NativeV8EngineProxy = V8NetProxy.CreateV8EngineProxy(false, null, 0);

                _RegisterEngine(_NativeV8EngineProxy->ID);

                _GlobalObjectTemplateProxy = CreateObjectTemplate<ObjectTemplate>(false);

                if (autoCreateGlobalContext)
                {
                    _NativeContext = V8NetProxy.CreateContext(_NativeV8EngineProxy, _GlobalObjectTemplateProxy._NativeObjectTemplateProxy);
                    _Context = new Context(_NativeContext);
                    _GlobalObject = new InternalHandle(V8NetProxy.SetContext(_NativeV8EngineProxy, _NativeContext), true); // (returns the global object handle)
                }
            }

            ___V8GarbageCollectionRequestCallback = _V8GarbageCollectionRequestCallback;
            V8NetProxy.RegisterGCCallback(_NativeV8EngineProxy, ___V8GarbageCollectionRequestCallback);

            _Initialize_Handles();
            _Initialize_ObjectTemplate();
            //?_Initialize_Worker(); // (DO THIS LAST!!! - the worker expects everything to be ready)


            _jsConverter = jsConverter ?? V8Engine._dummyJsConverter;

            TypeMappers = new Dictionary<Type, Func<object, InternalHandle>>()
            {
                {typeof(bool), (v) => CreateValue((bool) v)},
                {typeof(byte), (v) => CreateValue((byte) v)},
                {typeof(char), (v) => CreateValue((char) v)},
                {typeof(TimeSpan), (v) => CreateValue((TimeSpan) v)},
                {typeof(DateTime), (v) => CreateValue((DateTime) v)},
                //{typeof(DateTimeOffset), (v) => engine.Realm.Intrinsics.Date.Construct((DateTimeOffset) v)},
                {typeof(decimal), (v) => CreateValue((double) (decimal) v)},
                {typeof(double), (v) => CreateValue((double) v)},
                {typeof(SByte), (v) => CreateValue((Int32) (SByte) v)},
                {typeof(Int16), (v) => CreateValue((Int32) (Int16) v)}, 
                {typeof(Int32), (v) => CreateValue((Int32) v)},
                {typeof(Int64), (v) => CreateIntValue((Int64) v)},
                {typeof(Single), (v) => CreateValue((double) (Single) v)},
                {typeof(string), (v) => CreateValue((string) v)},
                {typeof(UInt16), (v) => CreateUIntValue((UInt16) v)}, 
                {typeof(UInt32), (v) => CreateUIntValue((UInt32) v)},
                {typeof(UInt64), (v) => CreateUIntValue((UInt64) v)},
                {
                    typeof(System.Text.RegularExpressions.Regex),
                    (v) => CreateValue((System.Text.RegularExpressions.Regex) v)
                }
            };

        }

        ~V8Engine()
        {
            Dispose();
        }

        public virtual void Dispose()
        {
            if (_NativeV8EngineProxy != null)
            {
                //?_TerminateWorker(); // (will return only when it has successfully terminated)

                // ... clear all handles of object IDs for disposal ...


                for (var i = 0; i < _TrackerHandles.Length; i++)
                {
                    var cref = _TrackerHandles[i];
                    cref?.Dispose();
                }

                HandleProxy* hProxy;

                for (var i = 0; i < _HandleProxies.Length; i++)
                {
                    hProxy = _HandleProxies[i];
                    if (hProxy != null && !hProxy->IsDisposed)
                    {
                        hProxy->_ObjectID = -2; // (note: this must be <= -2, otherwise the ID auto updates -1 to -2 to flag the ID as already processed)
                        hProxy->ManagedReference = 0;
                        V8NetProxy.DisposeHandleProxy(hProxy);
                        _HandleProxies[i] = null;
                    }
                }

                // ... allow all objects to be finalized by the GC ...

                RootableReference rref;
                V8NativeObject obj;

                for (var i = 0; i < _Objects.Count; i++) {
                    if ((rref = _Objects[i]) != null && (obj = (V8NativeObject)rref.Target) != null)
                    {
                        obj.OnDispose();
                        obj._ID = null;
                        obj.Template = null;
                        obj._Handle = InternalHandle.Empty;
                        GC.SuppressFinalize(obj);
                    }
                }

                _GlobalObject.Dispose();

                // ... destroy the native engine ...
                // TODO: Consider caching the engine instead and reuse with a new context.

                if (_NativeV8EngineProxy != null)
                {
                    _Engines[_NativeV8EngineProxy->ID] = null; // (notifies any lingering handles that this engine is now gone)
                    V8NetProxy.DestroyV8EngineProxy(_NativeV8EngineProxy);
                    _NativeV8EngineProxy = null;
                }
            }
        }

        /// <summary>
        /// Returns true once this engine has been disposed.
        /// </summary>
        public bool IsDisposed { get { return _NativeV8EngineProxy == null; } }


        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Sets V8 command line options.
        /// </summary>
        /// <param name="flags">Command line options/flags separated by a space.</param>
        public void SetFlagsFromString(string flags) // (http://bespin.cz/~ondras/html/classv8_1_1V8.html#ab263a85e6f97ea79d944bd20bb09a95f)
        {
            V8NetProxy.SetFlagsFromString(_NativeV8EngineProxy, flags);
        }

        /// <summary>
        /// Sets V8 command line options.
        /// <para>Just a convenient way to call 'SetFlagsFromString()' by joining all strings together, delimited by a space.</para>
        /// </summary>
        public void SetFlagsFromCommandLine(string[] args) // (http://bespin.cz/~ondras/html/classv8_1_1V8.html#a63157ad9284ffad1c0ab62b21aadd08c)
        {
            SetFlagsFromString(string.Join(" ", args));
        }

        // --------------------------------------------------------------------------------------------------------------------

        public bool ManagedSideHasDisposedHandles = false;

        /// <summary>
        ///     Calling this method forces a native call to 'LowMemoryNotification()' and 'IdleNotificationDeadline()' to push the
        ///     V8 engine to complete garbage collection tasks. The work performed helps to reduce the memory footprint within the
        ///     native V8 engine.
        ///     <para>(See also: <seealso cref="DoIdleNotification(int)"/>)</para>
        ///     <para>Note: You CANNOT GC CLR objects using this method.  This only applies to collection of native V8 handles that
        ///     are no longer in use. To *force* the disposal of an object, do this: "{Handle}.ReleaseManagedObject();
        ///     {Handle}.Dispose(); GC.Collect();
        ///     "</para>
        /// </summary>
        public void ForceV8GarbageCollection()
        {
            if (IsMemoryChecksOn)
            {
                do {
                    ManagedSideHasDisposedHandles = false;
                    V8NetProxy.ForceGC(_NativeV8EngineProxy);
                } while (ManagedSideHasDisposedHandles);
            }
        }

        public void ForceV8GarbageCollectionIfDisposed()
        {
            if (ManagedSideHasDisposedHandles) {
                ForceV8GarbageCollection();
            }
        }

        public void AddToMemorySnapshots(InternalHandle h)
        {
            if (IsMemoryChecksOn)
            {
                foreach(var snapshot in MemorySnapshots.Values)
                {
                    snapshot.Add(h);
                }
            }

        }

        /// <summary>
        ///     Calling this method notifies the native V8 engine to perform work tasks before returning. The delay is the amount of
        ///     time given to V8 to complete it's tasks, such as garbage collection. The work performed helps to reduce the memory
        ///     footprint within V8. This helps the garbage collector know when to start collecting objects and values that are no
        ///     longer in use. A true returned value indicates that V8 has done as much cleanup as it will be able to do.
        ///     <para>(See also: <seealso cref="ForceV8GarbageCollection()"/>)</para>
        /// </summary>
        /// <param name="delay">
        ///     (Optional) The amount of time, in seconds, allocated to the V8 GC to run some tasks, such as garbage collection.
        ///     V8.Net defaults this to 1 second.
        /// </param>
        /// <returns> False if more work is pending, and true when all work is completed (nothing more to do). </returns>
        public bool DoIdleNotification(int delay = 1)
        {
            return V8NetProxy.DoIdleNotification(_NativeV8EngineProxy, delay);
        }

        bool _V8GarbageCollectionRequestCallback(HandleProxy* handleProxy)
        {
            if (handleProxy->_ObjectID >= 0)
            {
                return _UnrootObject(handleProxy->_ObjectID); // (prevent the V8 GC from disposing the handle; the managed object will now dispose of this handle when the MANAGED GC is ready)
            }
            return true; // (don't know what this is now, so allow the handle to be disposed)
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary> Executes JavaScript on the V8 engine and returns the result. </summary>
        /// <param name="script"> The script to run. </param>
        /// <param name="sourceName">
        ///     (Optional) A string that identifies the source of the script (handy for debug purposes).
        /// </param>
        /// <param name="throwExceptionOnError">
        ///     (Optional) If true, and the return value represents an error, an exception is thrown (default is 'false').
        /// </param>
        /// <param name="timeout">
        ///     (Optional) The amount of time, in milliseconds, to delay before 'TerminateExecution()' is invoked.
        /// </param>
        /// <param name="trackReturn">
        ///     (Optional) True to add a tracking object to the handle so the GC disposes of the native side automatically. Setting
        ///     this to false means you take full responsibility to dispose this manually. This also adds a very small speed boost
        ///     since no tracking is required.
        /// </param>
        /// <returns> An InternalHandle. </returns>
        public InternalHandle Execute(string script, string sourceName = "V8.NET", bool throwExceptionOnError = false, int timeout = 0)
        {
            Timer timer = null;

            if (timeout > 0)
                timer = new Timer((state) => { TerminateExecution(); }, this, timeout, Timeout.Infinite);

            InternalHandle result = V8NetProxy.V8Execute(_NativeV8EngineProxy, script, sourceName);
            // (note: speed is not an issue when executing whole scripts, so the result is returned in a handle object instead of a value [safer])

            if (timer != null)
                timer.Dispose();

            if (throwExceptionOnError)
                result.ThrowOnError();

            return result;
        }

        /// <summary> Executes JavaScript on the V8 engine and returns the result. </summary>
        /// <exception cref="InvalidOperationException"> Thrown when the requested operation is invalid. </exception>
        /// <param name="script"> The script to run. </param>
        /// <param name="throwExceptionOnError">
        ///     (Optional) If true, and the return value represents an error, an exception is thrown (default is 'false').
        /// </param>
        /// <param name="timeout">
        ///     (Optional) The amount of time, in milliseconds, to delay before 'TerminateExecution()' is invoked.
        /// </param>
        /// <returns> An InternalHandle. </returns>
        public InternalHandle Execute(InternalHandle script, bool throwExceptionOnError = false, int timeout = 0)
        {
            if (script.ValueType != JSValueType.Script)
                throw new InvalidOperationException("The handle must represent pre-compiled JavaScript.");

            Timer timer = null;

            if (timeout > 0)
                timer = new Timer((state) => { TerminateExecution(); }, this, timeout, Timeout.Infinite);

            InternalHandle result = V8NetProxy.V8ExecuteCompiledScript(_NativeV8EngineProxy, script);
            // (note: speed is not an issue when executing whole scripts, so the result is returned in a handle object instead of a value [safer])

            if (timer != null)
                timer.Dispose();

            if (throwExceptionOnError)
                result.ThrowOnError();

            return result;
        }

        /// <summary>
        ///     Executes JavaScript on the V8 engine and automatically writes the result to the console (only valid for applications
        ///     that support 'Console' methods).
        ///     <para>Note: This is just a shortcut to calling 'Execute()' followed by 'Console.WriteLine()'.</para>
        /// </summary>
        /// <param name="script"> The script to run. </param>
        /// <param name="sourceName">
        ///     (Optional) A string that identifies the source of the script (handy for debug purposes).
        /// </param>
        /// <param name="throwExceptionOnError">
        ///     (Optional) If true, and the return value represents an error, an exception is thrown (default is 'false').
        /// </param>
        /// <param name="timeout">
        ///     (Optional) The amount of time, in milliseconds, to delay before 'TerminateExecution()' is invoked.
        /// </param>
        /// <returns> The result of the executed script. </returns>
        public InternalHandle ConsoleExecute(string script, string sourceName = "V8.NET", bool throwExceptionOnError = false, int timeout = 0)
        {
            InternalHandle result = Execute(script, sourceName, throwExceptionOnError, timeout);
            Console.WriteLine(result.AsString);
            return result;
        }

        /// <summary>
        ///     Executes JavaScript on the V8 engine and automatically writes the script given AND the result to the console (only
        ///     valid for applications that support 'Console' methods). The script is output to the console window before it gets
        ///     executed.
        ///     <para>Note: This is just a shortcut to calling 'Console.WriteLine(script)', followed by 'ConsoleExecute()'.</para>
        /// </summary>
        /// <param name="script"> The script to run. </param>
        /// <param name="sourceName">
        ///     (Optional) A string that identifies the source of the script (handy for debug purposes).
        /// </param>
        /// <param name="throwExceptionOnError">
        ///     (Optional) If true, and the return value represents an error, an exception is thrown (default is 'false').
        /// </param>
        /// <param name="timeout">
        ///     (Optional) The amount of time, in milliseconds, to delay before 'TerminateExecution()' is invoked.
        /// </param>
        /// <returns> The result of the executed script. </returns>
        public InternalHandle VerboseConsoleExecute(string script, string sourceName = "V8.NET", bool throwExceptionOnError = false, int timeout = 0)
        {
            Console.WriteLine(script);
            InternalHandle result = Execute(script, sourceName, throwExceptionOnError, timeout);
            Console.WriteLine(result.AsString);
            return result;
        }

        /// <summary>
        ///     Compiles JavaScript on the V8 engine and returns the result. Since V8 JIT-compiles script every time, repeated tasks
        ///     can take advantage of re-executing pre-compiled scripts for a speed boost.
        /// </summary>
        /// <param name="script"> The script to run. </param>
        /// <param name="sourceName">
        ///     (Optional) A string that identifies the source of the script (handy for debug purposes).
        /// </param>
        /// <param name="throwExceptionOnError">
        ///     (Optional) If true, and the return value represents an error, an exception is thrown (default is 'false').
        /// </param>
        /// <returns> A handle to the compiled script. </returns>
        public InternalHandle Compile(string script, string sourceName = "V8.NET", bool throwExceptionOnError = false)
        {
            InternalHandle result = V8NetProxy.V8Compile(_NativeV8EngineProxy, script, sourceName);
            // (note: speed is not an issue when executing whole scripts, so the result is returned in a handle object instead of a value [safer])

            if (throwExceptionOnError)
                result.ThrowOnError();

            return result;
        }

        /// <summary>
        /// Forcefully terminate the current thread of JavaScript execution.
        /// This method can be used by any thread. 
        /// </summary>
        public void TerminateExecution()
        {
            V8NetProxy.TerminateExecution(_NativeV8EngineProxy);
        }

        /// <summary>
        /// Loads a JavaScript file from the current working directory (or specified absolute path) and executes it in the V8 engine, then returns the result.
        /// </summary>
        /// <param name="scriptFile">The script file to load.</param>
        /// <param name="sourceName">A string that identifies the source of the script (handy for debug purposes).</param>
        /// <param name="throwExceptionOnError">If true, and the return value represents an error, or the file fails to load, an exception is thrown (default is 'false').</param>
        public InternalHandle LoadScript(string scriptFile, string sourceName = null, bool throwExceptionOnError = false)
        {
            InternalHandle result;
            try
            {
                var jsText = File.ReadAllText(scriptFile);
                result = Execute(jsText, sourceName ?? scriptFile, throwExceptionOnError);
                if (throwExceptionOnError)
                    result.ThrowOnError();
            }
            catch (Exception ex)
            {
                if (throwExceptionOnError)
                    throw ex;
                result = CreateValue(Exceptions.GetFullErrorMessage(ex));
                result._HandleProxy->_Type = JSValueType.InternalError; // (required to flag that an error has occurred)
            }
            return result.KeepTrack();
        }

        /// <summary>
        /// Loads a JavaScript file from the current working directory (or specified absolute path) and compiles it in the V8 engine, then returns the compiled script.
        /// You will need to call 'Execute(...)' with the script handle to execute it.
        /// </summary>
        /// <param name="scriptFile">The script file to load.</param>
        /// <param name="sourceName">A string that identifies the source of the script (handy for debug purposes).</param>
        /// <param name="throwExceptionOnError">If true, and the return value represents an error, or the file fails to load, an exception is thrown (default is 'false').</param>
        public InternalHandle LoadScriptCompiled(string scriptFile, string sourceName = "V8.NET", bool throwExceptionOnError = false)
        {
            InternalHandle result;
            try
            {
                var jsText = File.ReadAllText(scriptFile);
                result = Compile(jsText, sourceName, throwExceptionOnError);
                if (throwExceptionOnError)
                    result.ThrowOnError();
            }
            catch (Exception ex)
            {
                if (throwExceptionOnError)
                    throw ex;
                result = CreateValue(Exceptions.GetFullErrorMessage(ex));
                result._HandleProxy->_Type = JSValueType.InternalError; // (required to flag that an error has occurred)
            }
            return result.KeepTrack();
        }
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Creates a new native V8 ObjectTemplate and associates it with a new managed ObjectTemplate.
        /// <para>Object templates are required in order to generate objects with property interceptors (that is, all property access is redirected to the managed side).</para>
        /// </summary>
        /// <param name="registerPropertyInterceptors">If true (default) then property interceptors (call-backs) will be used to support 'IV8ManagedObject' objects.
        /// <para>Note: Setting this to false provides a huge performance increase because all properties will be stored on the native side only (but 'IV8ManagedObject'
        /// objects created by this template will not intercept property access).</para></param>
        /// <typeparam name="T">Normally this is always 'ObjectTemplate', unless you have a derivation of it.</typeparam>
        public T CreateObjectTemplate<T>(bool registerPropertyInterceptors = true) where T : ObjectTemplate, new()
        {
            var template = new T();
            template._Initialize(this, registerPropertyInterceptors);
            return template;
        }

        /// <summary>
        /// Creates a new native V8 ObjectTemplate and associates it with a new managed ObjectTemplate.
        /// <para>Object templates are required in order to generate objects with property interceptors (that is, all property access is redirected to the managed side).</para>
        /// </summary>
        public ObjectTemplate CreateObjectTemplate() { return CreateObjectTemplate<ObjectTemplate>(); }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Creates a new native V8 FunctionTemplate and associates it with a new managed FunctionTemplate.
        /// <para>Function templates are required in order to associated managed delegates with JavaScript functions within V8.</para>
        /// </summary>
        /// <typeparam name="T">Normally this is always 'FunctionTemplate', unless you have a derivation of it.</typeparam>
        /// <param name="className">The "class name" in V8 is the type name returned when "valueOf()" is used on an object. If this is null then 'V8Function' is assumed.</param>
        /// <param name="callbackSource">A delegate to call when the function is executed.</param>
        public T CreateFunctionTemplate<T>(string className) where T : FunctionTemplate, new()
        {
            var template = new T();
            template._Initialize(this, className ?? typeof(V8Function).Name);
            return template;
        }

        /// <summary>
        /// Creates a new native V8 FunctionTemplate and associates it with a new managed FunctionTemplate.
        /// <para>Function templates are required in order to associated managed delegates with JavaScript functions within V8.</para>
        /// </summary>
        /// <param name="className">The "class name" in V8 is the type name returned when "valueOf()" is used on an object. If this is null (default) then 'V8Function' is assumed.</param>
        /// <param name="callbackSource">A delegate to call when the function is executed.</param>
        public FunctionTemplate CreateFunctionTemplate(string className = null)
        {
            return CreateFunctionTemplate<FunctionTemplate>(className);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        ///     Calls the native V8 proxy library to create a new context environment for use with executing JavaScript.
        /// </summary>
        /// <param name="globalTemplate"> (Optional) An optional global template (if null one will be created automatically). </param>
        /// <returns> A context wrapper the references the native V8 context. </returns>
        public Context CreateContext(ObjectTemplate globalTemplate = null)
        {
            return V8NetProxy.CreateContext(_NativeV8EngineProxy, globalTemplate != null ? globalTemplate._NativeObjectTemplateProxy : null);
        }

        /// <summary>
        ///     Returns the current context.
        /// </summary>
        /// <returns> The current V8 context. </returns>
        public Context GetContext()
        {
            return V8NetProxy.GetContext(_NativeV8EngineProxy);
        }

        /// <summary>
        ///     Changes the V8 proxy engine context to a new execution context. Each context can be used as a "sandbox" to isolate
        ///     executions from one another. This method also returns the global object handle associated with the context.
        ///     <para>In the normal "V8" way, you enter a context before executing JavaScript, then exit a context when done.  To
        ///     limit the overhead of calling from managed methods into native functions for entering and exiting contexts all the
        ///     time, V8.Net instead sets the current context on a native V8 engine proxy so all future calls use the new context.
        ///     The context will be entered and exited automatically on the native side as needed. </para>
        /// </summary>
        /// <param name="context"> The new context to set. </param>
        /// <returns> An InternalHandle. </returns>
        public InternalHandle SetContext(Context context)
        {
            var hglobal = V8NetProxy.SetContext(_NativeV8EngineProxy, context); // (returns the global object handle)
            _Context = context;
            _NativeContext = context;
            _GlobalObject.KeepTrack(); // (not sure if the user will keep track of the internal handle, so we will let the GC track it just in case)
            _GlobalObject = hglobal; // (just replace it)
            return _GlobalObject;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the native V8 proxy library to create the value instance for use within the V8 JavaScript environment.
        /// </summary>
        public InternalHandle CreateValue(bool b) { return new InternalHandle(V8NetProxy.CreateBoolean(_NativeV8EngineProxy, b), true); }

        /// <summary>
        /// Calls the native V8 proxy library to create a 32-bit integer for use within the V8 JavaScript environment.
        /// </summary>
        public InternalHandle CreateValue(Int32 num) { return new InternalHandle(V8NetProxy.CreateInteger(_NativeV8EngineProxy, num), true); }

        /// <summary>
        /// Calls the native V8 proxy library to create a 64-bit number (double) for use within the V8 JavaScript environment.
        /// </summary>
        public InternalHandle CreateValue(double num) { return new InternalHandle(V8NetProxy.CreateNumber(_NativeV8EngineProxy, num), true); }

        /// <summary>
        /// Calls the native V8 proxy library to create a string for use within the V8 JavaScript environment.
        /// </summary>
        public InternalHandle CreateValue(string str) { return new InternalHandle(V8NetProxy.CreateString(_NativeV8EngineProxy, str), true); }

        /// <summary>
        /// Calls the native V8 proxy library to create an error string for use within the V8 JavaScript environment.
        /// <para>Note: The error flag exists in the associated proxy object only.  If the handle is passed along to another operation, only the string message will get passed.</para>
        /// </summary>
        public InternalHandle CreateError(string message, JSValueType errorType)
        {
            if (errorType >= 0) throw new InvalidOperationException("Invalid error type.");
            return new InternalHandle(V8NetProxy.CreateError(_NativeV8EngineProxy, message, errorType), true);
        }

        /// <summary>
        /// Calls the native V8 proxy library to create a date for use within the V8 JavaScript environment.
        /// </summary>
        /// <param name="ms">The number of milliseconds since epoch (Jan 1, 1970). This is the same value as 'SomeDate.getTime()' in JavaScript.</param>
        public InternalHandle CreateValue(TimeSpan ms) { return new InternalHandle(V8NetProxy.CreateDate(_NativeV8EngineProxy, ms.TotalMilliseconds), true); }

        /// <summary>
        /// Calls the native V8 proxy library to create a date for use within the V8 JavaScript environment.
        /// </summary>
        public InternalHandle CreateValue(DateTime date) { return CreateValue(date.ToUniversalTime().Subtract(Epoch)); }

        /// <summary>
        /// Wraps a given object handle with a managed object, and optionally associates it with a template instance.
        /// <para>Note: Any other managed object associated with the given handle will cause an error.
        /// You should check '{Handle}.HasManagedObject', or use the "GetObject()" methods to make sure a managed object doesn't already exist.</para>
        /// <para>This was method exists to support the following cases: 1. The V8 context auto-generates the global object, and
        /// 2. V8 function objects are not generated from templates, but still need a managed wrapper.</para>
        /// <para>Note: </para>
        /// </summary>
        /// <typeparam name="T">The wrapper type to create (such as V8ManagedObject).</typeparam>
        /// <param name="v8Object">A handle to a native V8 object.</param>
        /// <param name="initialize">If true (default) then then 'IV8NativeObject.Initialize()' is called on the created object before returning.</param>
        internal T _CreateObject<T>(ITemplate template, InternalHandle v8Object, bool initialize = true, bool connectNativeObject = true)
            where T : V8NativeObject, new()
        {
            if (!v8Object.IsObjectType)
                throw new InvalidOperationException("An object handle type is required (such as a JavaScript object or function handle).");

            // ... create the new managed JavaScript object, store it (to get the "ID"), and connect it to the native V8 object ...
            var obj = _CreateManagedObject<T>(template, v8Object, connectNativeObject);

            if (initialize)
                obj.Initialize(false, null);

            return obj;
        }

        /// <summary>
        /// Wraps a given object handle with a managed object.
        /// <para>Note: Any other managed object associated with the given handle will cause an error.
        /// You should check '{Handle}.HasManagedObject', or use the "GetObject()" methods to make sure a managed object doesn't already exist.</para>
        /// <para>This was method exists to support the following cases: 1. The V8 context auto-generates the global object, and
        /// 2. V8 function objects are not generated from templates, but still need a managed wrapper.</para>
        /// <para>Note: </para>
        /// </summary>
        /// <typeparam name="T">The wrapper type to create (such as V8ManagedObject).</typeparam>
        /// <param name="v8Object">A handle to a native V8 object.</param>
        /// <param name="initialize">If true (default) then then 'IV8NativeObject.Initialize()' is called on the created object before returning.</param>
        public T CreateObject<T>(InternalHandle v8Object, bool initialize = true)
            where T : V8NativeObject, new()
        {
            return _CreateObject<T>(null, v8Object, initialize);
        }

        /// <summary>
        /// See <see cref="CreateObject&lt;T>(InternalHandle, bool)"/>.
        /// </summary>
        public V8NativeObject CreateObject(InternalHandle v8Object, bool initialize = true) { return CreateObject<V8NativeObject>(v8Object, initialize); }

        /// <summary>
        /// Creates a new CLR object which will be tracked by a new V8 native object.
        /// </summary>
        /// <param name="initialize">If true (default) then then 'IV8NativeObject.Initialize()' is called on the created wrapper before returning.</param>
        /// <typeparam name="T">A custom 'V8NativeObject' type, or just use 'V8NativeObject' as a default.</typeparam>
        public T CreateObject<T>(bool initialize = true)
            where T : V8NativeObject, new()
        {
            // ... create the new managed JavaScript object and store it (to get the "ID")...
            var obj = _CreateManagedObject<T>(null, null);

            try
            {
                // ... create a new native object and associated it with the new managed object ID ...
                obj._Handle.Set(new InternalHandle(V8NetProxy.CreateObject(_NativeV8EngineProxy, obj.ID), true));

                /* The V8 object will have an associated internal field set to the index of the created managed object above for quick lookup.  This index is used
                 * to locate the associated managed object when a call-back occurs. The lookup is a fast O(1) operation using the custom 'IndexedObjectList' manager.
                 */
            }
            catch (Exception ex)
            {
                // ... something went wrong, so remove the new managed object ...
                _RemoveObjectRootableReference(obj.ID);
                throw ex;
            }

            if (initialize)
                obj.Initialize(false, null);

            return (T)obj;
        }

        /// <summary>
        /// Creates a new native V8 object only.
        /// </summary>
        public InternalHandle CreateObject()
        {
            //x if (objectID > -2) throw new InvalidOperationException("Object IDs must be <= -2.");
            return new InternalHandle(V8NetProxy.CreateObject(_NativeV8EngineProxy, -1), true); // TODO: Consider associating a user-defined string value to store instead.
        }

        /// <summary>
        /// Calls the native V8 proxy library to create a JavaScript array for use within the V8 JavaScript environment.
        /// <para>Note: The given handles are not disposed, and the caller is still responsible.</para>
        /// </summary>
        public InternalHandle CreateArray(params InternalHandle[] items)
        {
            HandleProxy** nativeArrayMem = items.Length > 0 ? Utilities.MakeHandleProxyArray(items) : null;

            InternalHandle result = new InternalHandle(V8NetProxy.CreateArray(_NativeV8EngineProxy, nativeArrayMem, items.Length), true);

            Utilities.FreeNativeMemory((IntPtr)nativeArrayMem);

            return result;
        }

        /// <summary>
        /// Converts an enumeration of values (usually from a collection, list, or array) into a JavaScript array.
        /// By default, an exception will occur if any type cannot be converted.
        /// </summary>
        /// <param name="enumerable">An enumerable object to convert into a native V8 array.</param>
        /// <returns>A native V8 array.</returns>
        public InternalHandle CreateValue(IEnumerable enumerable, bool ignoreErrors = false)
        {
            var values = (enumerable).Cast<object>().ToArray();
            var maxQuickInitLength = values.Length < 1000 ? values.Length : 1000;

            InternalHandle[] handles = new InternalHandle[maxQuickInitLength];

            for (var i = 0; i < maxQuickInitLength; i++)
                try { handles[i] = CreateValue(values[i]); }
                catch (Exception ex) { if (!ignoreErrors) throw ex; }

            InternalHandle array = CreateArray(handles); // (faster to initialize on the native side all at once, which is fine for a small number of handles)

            // .. must dispose the internal handles now ...

            for (var i = 0; i < maxQuickInitLength; i++)
                handles[i].Dispose();

            // ... if the shear number of items is large it will cause a spike in handle counts, so set the remaining items one by one instead on a native array ...

            var remainingItemLength = values.Length - maxQuickInitLength;
            if (remainingItemLength > 0)
            {
                for (var i = maxQuickInitLength; i < values.Length; i++)
                    try { array.SetProperty(i, CreateValue(values[i])); }
                    catch (Exception ex) { if (!ignoreErrors) throw ex; }
            }

            return array;
        }

        /// <summary>
        /// Calls the native V8 proxy library to create the value instance for use within the V8 JavaScript environment.
        /// <para>This overload provides a *quick way* to construct an array of strings.
        /// One big memory block is created to marshal the given strings at one time, which is many times faster than having to create an array of individual native strings.</para>
        /// </summary>
        public InternalHandle CreateValue(IEnumerable<string> items)
        {
            var _items = items?.ToArray(); // (the enumeration could be lengthy depending on the implementation, so iterate it only once and dump it to an array)
            if (_items == null || _items.Length == 0) return new InternalHandle(V8NetProxy.CreateArray(_NativeV8EngineProxy, null, 0), true);

            int strBufSize = 0; // (size needed for the string chars portion of the memory block)
            int itemsCount = 0;

            for (int i = 0, n = _items.Length; i < n; ++i)
            {
                // get length of all strings together
                strBufSize += _items[i].Length + 1; // (+1 for null char)
                itemsCount++;
            }

            int strPtrBufSize = Marshal.SizeOf(typeof(IntPtr)) * itemsCount; // start buffer size with size needed for all string pointers.
            char** oneBigStringBlock = (char**)Utilities.AllocNativeMemory(strPtrBufSize + Marshal.SystemDefaultCharSize * strBufSize);
            char** ptrWritePtr = oneBigStringBlock;
            char* strWritePtr = (char*)(((byte*)oneBigStringBlock) + strPtrBufSize);
            int itemLength;

            for (int i = 0, n = _items.Length; i < n; ++i)
            {
                itemLength = _items[i].Length;
                Marshal.Copy(_items[i].ToCharArray(), 0, (IntPtr)strWritePtr, itemLength);
                Marshal.WriteInt16((IntPtr)(strWritePtr + itemLength), 0);
                Marshal.WriteIntPtr((IntPtr)ptrWritePtr++, (IntPtr)strWritePtr);
                strWritePtr += itemLength + 1;
            }

            InternalHandle handle = new InternalHandle(V8NetProxy.CreateStringArray(_NativeV8EngineProxy, oneBigStringBlock, itemsCount), true);

            Utilities.FreeNativeMemory((IntPtr)oneBigStringBlock);

            return handle;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Simply creates and returns a 'null' JavaScript value.
        /// </summary>
        public InternalHandle CreateNullValue() { return new InternalHandle(V8NetProxy.CreateNullValue(_NativeV8EngineProxy), true); }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Creates a native V8 JavaScript value that the best represents the given managed value.
        /// Object instance values will be bound to a 'V8NativeObject' wrapper and returned.
        /// To include implicit wrapping of object-type fields and properties for object instances, set 'recursive' to true, otherwise they will be skipped.
        /// <para>Warning: Integers are 32-bit, and Numbers (double) are 64-bit.  This means converting 64-bit integers may result in data loss.</para>
        /// </summary>
        /// <param name="value">One of the supported value types: bool, byte, Int16-64, Single, float, double, string, char, StringBuilder, DateTime, or TimeSpan. (Warning: Int64 will be converted to Int32 [possible data loss])</param>
        /// <param name="recursive">For object instances, if true, then nested objects are included, otherwise only the object itself is bound and returned.
        /// For security reasons, public members that point to object instances will be ignored. This must be true to included those as well, effectively allowing
        /// in-script traversal of the object reference tree (so make sure this doesn't expose sensitive methods/properties/fields).</param>
        /// <param name="memberSecurity">For object instances, these are default flags that describe JavaScript properties for all object instance members that
        /// don't have any 'ScriptMember' attribute.  The flags should be 'OR'd together as needed.</param>
        /// <returns>A native value that best represents the given managed value.</returns>
        public InternalHandle CreateValue(object value, bool? recursive = null, ScriptMemberSecurity? memberSecurity = null)
        {
            var jsRes = InternalHandle.Empty;
            if (value == null)
                jsRes = CreateNullValue();
            else if (value is IHandleBased)
                jsRes = ((IHandleBased)value).InternalHandle; // (already a V8.NET value!)
            else if (value is bool)
                jsRes = CreateValue((bool)value);
            else if (value is byte)
                jsRes = CreateValue((Int32)(byte)value);
            else if (value is sbyte)
                jsRes = CreateValue((Int32)(sbyte)value);
            else if (value is Int16)
                jsRes = CreateValue((Int32)(Int16)value);
            else if (value is UInt16)
                jsRes = CreateValue((Int32)(UInt16)value);
            else if (value is Int32)
                jsRes = CreateValue((Int32)value);
            else if (value is UInt32)
                jsRes = CreateValue((double)(UInt32)value);
            else if (value is Int64)
                jsRes = CreateValue((double)(Int64)value); // (warning: data loss may occur when converting 64int->64float)
            else if (value is UInt64)
                jsRes = CreateValue((double)(UInt64)value); // (warning: data loss may occur when converting 64int->64float)
            else if (value is Single)
                jsRes = CreateValue((double)(Single)value);
            else if (value is float)
                jsRes = CreateValue((double)(float)value);
            else if (value is double)
                jsRes = CreateValue((double)value);
            else if (value is string)
                jsRes = CreateValue((string)value);
            else if (value is char)
                jsRes = CreateValue(((char)value).ToString());
            else if (value is StringBuilder)
                jsRes = CreateValue(((StringBuilder)value).ToString());
            else if (value is DateTime)
                jsRes = CreateValue((DateTime)value);
            else if (value is Enum) // (enums are simply integer like values)
                jsRes = CreateValue((int)value);
            else if (value is Array)
                jsRes = CreateValue((IEnumerable)value);
            else
            {
                jsRes = FromObject(value, createBinder: false);
                if (jsRes.IsEmpty)
                {
                    if (value is TimeSpan)
                        jsRes = CreateValue((TimeSpan)value);
                    else //??if (value.GetType().IsClass)
                        jsRes = CreateBinding(value, null, recursive, memberSecurity);
                }
            }

            if (jsRes.IsEmpty) {
                throw new NotSupportedException($"Cannot convert object of type '{value.GetType().Name}' to a JavaScript value.");
            }

            if (IsMemoryChecksOn)
            {
                int expectedRefCount = 1 + (jsRes.IsRooted ? 1 : 0);
                if (jsRes.RefCount != expectedRefCount)
                    throw new InvalidOperationException($"Create value wrong ref count (not {expectedRefCount}): {jsRes.RefCount}");
            }
            
            return jsRes;
        }


        public InternalHandle CreateClrCallBack(JSFunction func, bool keepAlive = true)
        {
            var jsFunc = CreateFunctionTemplate().GetFunctionObject<V8Function>(func)._;
            if (keepAlive)
                jsFunc.KeepAlive();
            return jsFunc;
        }

        public static void Dispose(InternalHandle[] jsItems)
        {
            for (int i = 0; i < jsItems.Length; ++i)
            {
                jsItems[i].KeepAlive().Dispose();
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public InternalHandle CreateEmptyArray()
        {
            return CreateArray(Array.Empty<InternalHandle>());
        }

        public InternalHandle CreateArrayWithDisposal(InternalHandle[] jsItems)
        {
            var jsArr = CreateArray(jsItems);
            Dispose(jsItems);
            return jsArr;
        }

        // --------------------------------------------------------------------------------------------------------------------

        public MemorySnapshot LastMemorySnapshotBefore
        {

            get => _Context.LastMemorySnapshotBefore;
            set
            {
                _Context.LastMemorySnapshotBefore = value;
            }
        }

        public Dictionary<string, MemorySnapshot> MemorySnapshots
        {

            get => _Context.MemorySnapshots;
            set
            {
                _Context.MemorySnapshots = value;
            }
        }

        public void AddToLastMemorySnapshotBefore(InternalHandle h)
        {
            if (IsMemoryChecksOn)
                LastMemorySnapshotBefore?.Add(h);
        }

        public MemorySnapshot MakeMemorySnapshot(string name)
        {
            if (!IsMemoryChecksOn)
                return null;
        
            MemorySnapshot snapshotBefore = null;
            if (MemorySnapshots?.TryGetValue(name, out snapshotBefore) == true &&
                snapshotBefore != null)
            {
                snapshotBefore.Init(this);
            }
            else {
                snapshotBefore = new MemorySnapshot(this);
                MemorySnapshots[name] = snapshotBefore;
            }
            LastMemorySnapshotBefore = snapshotBefore;
            return snapshotBefore;
        }

        public bool RemoveMemorySnapshot(string name)
        {
            return MemorySnapshots.Remove(name);
        }

        public void CheckForMemoryLeaks(string name, bool shouldRemove = true)
        {
            if (!IsMemoryChecksOn)
                return;
                
            if (MemorySnapshots == null)
                MemorySnapshots = new Dictionary<string, MemorySnapshot>();
                
            MemorySnapshot snapshotBefore = null;
            if (!(MemorySnapshots.TryGetValue(name, out snapshotBefore) == true &&
                snapshotBefore != null))
            {
                throw new InvalidOperationException($"CheckForMemoryLeaks: No snapshotBefore named {name} exists");
            }

            if (shouldRemove)
                RemoveMemorySnapshot(name);

            string leakagesDescHandles = "";
            string leakagesDescObjects = "";
            string leakagesDescSubobjects = "";
            using (var jsStringify = this.Execute("JSON.stringify", "JSON.stringify", true, 0))
            {
                snapshotBefore.Add(jsStringify);

                var snapshotAfter = new MemorySnapshot(this);
                
                foreach (var i in snapshotAfter.ExistingHandleIDs)
                {
                    var hProxy = _HandleProxies[i];
                    if (hProxy != null)
                    {
                        var h = new InternalHandle(hProxy, false);
                        _GatherChilds(snapshotAfter, ref h);
                    }
                }
                ForceV8GarbageCollection(); // here the suspected handle may be disposed by V8 so we skip them if this has happened

                foreach (var i in snapshotAfter.ExistingHandleIDs)
                {
                    var hProxy = _HandleProxies[i];
                    if (hProxy != null)
                    {
                        bool isLeaked = !hProxy->IsCLRDisposed && !snapshotBefore.ExistingHandleIDs.Contains(i);
                        if (isLeaked) {
                            var h = new InternalHandle(hProxy, false);
                            int refDiscount = h.IsRooted ? 1 : 0; //kvp.Value;
                            if (!h.IsCLRDisposed) {
                                int countParents = 0;
                                snapshotAfter.ChildHandleIDs.TryGetValue(h.HandleID, out countParents);
                                if (h.RefCount > refDiscount + countParents)
                                    leakagesDescHandles += _LeakageDesc(h, true, jsStringify);                        
                            }
                        }
                    }
                }

                Int32 objectsCount = 0;
                using (_ObjectsLocker.ReadLock()) { 
                    objectsCount = _Objects.Count;
                }

                foreach (var i in snapshotAfter.ExistingObjectIDs)
                {
                    V8NativeObject no = _GetExistingObject(i);
                    bool isLeaked = no != null && !snapshotBefore.ExistingObjectIDs.Contains(i);
                    if (isLeaked) {
                        InternalHandle h = InternalHandle.Empty;
                        if (isLeaked) {
                            h = no._;
                        }

                        if (!h.IsCLRDisposed) {
                            if (!h.IsEmpty) {
                                int refDiscount = h.IsRooted ? 1 : 0;
                                int countParents = 0;
                                snapshotAfter.ChildHandleIDs.TryGetValue(h.HandleID, out countParents);

                                if (h.RefCount > refDiscount + countParents)
                                    leakagesDescObjects += _LeakageDesc(h, true, jsStringify);                        
                            }
                            else {
                                leakagesDescObjects += $"objectID={i}\n";
                            }
                        }
                    }
                }
            }

            string leakagesDesc = "";
            if (leakagesDescHandles != "") {
                leakagesDesc += "\nLeaked internal handles:\n" + leakagesDescHandles;
            }
            if (leakagesDescObjects != "") {
                leakagesDesc += "\nLeaked managed objects:\n" + leakagesDescObjects;
            }

            if (leakagesDesc != "") {
                throw new InvalidOperationException($"Memory leakage in {name}: " + leakagesDesc);
            }
        }

        /*private InternalHandle _GetUltimateNonDisposedParent(MemorySnapshot snapshotBefore, ref InternalHandle h)
        {
            //InternalHandle res = h;
            while (!h.IsCLRDisposed && h.IsObjectType && h.RefCount >= 1)
            {
                object obj = null;
                if (h.IsBinder) {
                    obj = h.BoundObject;
                }
                else {
                    obj = h.Object;
                }
                IV8DebugInfo di = obj as IV8DebugInfo;
                if (di == null)
                    break;

                if (di.ParentID == null || di.ParentID.ObjectID < 0  || snapshotBefore.ExistingObjectIDs.Contains(di.ParentID.ObjectID))
                    break;
                V8NativeObject pno = _GetExistingObject(di.ParentID.ObjectID);
                if (pno == null)
                    break;
                h = pno._;
                ForceV8GarbageCollection();
            }
            return h;
        }*/

        private void _GatherChilds(MemorySnapshot snapshotBefore, ref InternalHandle h)
        {
            if (!(!h.IsCLRDisposed && h.IsObjectType && h.RefCount >= 1))
                return;

            object obj = null;
            if (h.IsBinder) {
                obj = h.BoundObject;
            }
            else {
                obj = h.Object;
            }
            IV8DebugInfo di = obj as IV8DebugInfo;
            if (di == null)
                return;

            List<V8EntityID> childIDs = di.ChildIDs;
            if (childIDs != null && !h.IsCLRDisposed) {
                foreach (var childID in childIDs) {
                    var childHandleID = childID.HandleID;
                    int countParents = 0;
                    snapshotBefore.ChildHandleIDs.TryGetValue(childHandleID, out countParents);
                    snapshotBefore.ChildHandleIDs[childHandleID] = countParents + 1;
                }
            }
        }


        private string _LeakageDesc(InternalHandle h, bool isLeaked, InternalHandle jsStringify)
        {
            string leakagesDesc = "";
            string summary = h.Summary;
            string errorKind = isLeaked ? "Leakage" : "Destroyed";
            leakagesDesc += $"{errorKind}: {summary}";

            if (false) 
            {
                using (var jsStrValue = jsStringify.StaticCall(h)) {
                    leakagesDesc += $", value={jsStrValue.AsString}";
                }
            }

            leakagesDesc += "\n";
            return leakagesDesc;
        }

    }

    public class V8EntityID
    {
        public Int32 HandleID;
        public Int32 ObjectID;

        public V8EntityID(Int32 handleID, Int32 objectID = -1)
        {
            HandleID = handleID;
            ObjectID = objectID;
        }
    }

    public interface IV8DebugInfo
    {
        V8EntityID SelfID {get; set;}
        V8EntityID ParentID {get;}
        List<V8EntityID> ChildIDs {get;}
        string Summary {get;}
    }

    // ========================================================================================================================
}
