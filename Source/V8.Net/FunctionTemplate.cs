﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

#if V2 || V3 || V3_5
#else
using System.Dynamic;
#endif

namespace V8.Net
{
    // ========================================================================================================================

    /// <summary>
    /// Represents a JavaScript callback function for a managed class method.
    /// </summary>
    /// <param name="isConstructCall">True only if this function is being called to construct a new object (such as using the "new" operator within JavaScript).
    /// If this is true, the function is expected to create and return a new object (as the constructor for that object).</param>
    /// <param name="args">The arguments supplied for the JavaScript function call.</param>
    public delegate InternalHandle JSFunction(V8Engine engine, bool isConstructCall, InternalHandle _this, params InternalHandle[] args);

    // ========================================================================================================================

    public unsafe class FunctionTemplate : TemplateBase<IV8Function>, IV8Disposable
    {
        // --------------------------------------------------------------------------------------------------------------------

        internal NativeFunctionTemplateProxy* _NativeFunctionTemplateProxy;

        public string ClassName { get; private set; }

        /// <summary>
        /// Set this to an object that implements a call-back to execute when the function associated with this FunctionTemplate is called within JavaScript.
        /// </summary>
        readonly Dictionary<Type, int> _FunctionsByType = new Dictionary<Type, int>();

        /// <summary>
        /// The V8 engine automatically creates two templates with every function template: one for object creation (instances) and one for function object itself (prototype inheritance).
        /// This property returns the ObjectTemplate wrapper associated with the V8 native instance template for creating new objects using the function in this template as the constructor.
        /// </summary>
        public ObjectTemplate InstanceTemplate { get; private set; }

        /// <summary>
        /// The V8 engine automatically creates two templates with every function template: one for object creation (instances) and one for object inheritance (prototypes).
        /// This property returns the ObjectTemplate wrapper associated with the prototype template for the function object in this template.
        /// </summary>
        public ObjectTemplate PrototypeTemplate { get; private set; }

        // --------------------------------------------------------------------------------------------------------------------

        public FunctionTemplate()
        {
        }

        /// <summary> Returns true if this template has child objects created from it. </summary>
        public override bool HasChildObjects
        {
            get
            {
                return base.HasChildObjects
                    || ((ITemplateInternal)PrototypeTemplate)._ReferenceCount >0
                    || ((ITemplateInternal)InstanceTemplate)._ReferenceCount > 0;
            }
        }

        protected override bool _Finalize(bool finalizer) // (note: This can cause issues if removed while the native object exists [because of the callbacks].)
        {
            if (_NativeFunctionTemplateProxy != null)
            {
                if (V8NetProxy.DeleteFunctionTemplateProxy(_NativeFunctionTemplateProxy)) // (delete the corresponding native object as well; WARNING: This is done on the GC thread!)
                {
                    _NativeFunctionTemplateProxy = null;

                    PrototypeTemplate.Parent = null;
                    InstanceTemplate.Parent = null;
                    PrototypeTemplate = null;
                    InstanceTemplate = null;
                }
                else
                    return false; // (bounced, a script might be in progress; try again later)
            }
            return true;
        }

        internal void _Initialize(V8Engine v8EngineProxy, string className)
        {
            ClassName = className;

            _Initialize(v8EngineProxy,
                (NativeFunctionTemplateProxy*)V8NetProxy.CreateFunctionTemplateProxy(
                    v8EngineProxy._NativeV8EngineProxy,
                    ClassName,
                    _SetDelegate<NativeFunctionCallback>(_CallBack)) // (create a corresponding native object)
            );
        }

        internal void _Initialize(V8Engine v8EngineProxy, NativeFunctionTemplateProxy* nativeFunctionTemplateProxy)
        {
            if (v8EngineProxy == null)
                throw new ArgumentNullException("v8EngineProxy");

            if (nativeFunctionTemplateProxy == null)
                throw new ArgumentNullException("nativeFunctionTemplateProxy");

            _Engine = v8EngineProxy;

            _NativeFunctionTemplateProxy = nativeFunctionTemplateProxy;

            InstanceTemplate = new ObjectTemplate();
            InstanceTemplate.Parent = this;
            InstanceTemplate._Initialize(_Engine, V8NetProxy.GetFunctionInstanceTemplateProxy(_NativeFunctionTemplateProxy), false);

            PrototypeTemplate = new ObjectTemplate();
            PrototypeTemplate.Parent = this;
            PrototypeTemplate._Initialize(_Engine, V8NetProxy.GetFunctionPrototypeTemplateProxy(_NativeFunctionTemplateProxy), false);

            OnInitialized();
        }

        /// <summary>
        /// Called when the object is initialized instance is ready for use.
        /// </summary>
        protected override void OnInitialized()
        {
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// This is the main entry point from the native side for all calls to functions created from function templates.
        /// This method exists to give an instance based delegate reference for each function template instance.
        /// </summary>
        /// <param name="managedObjectID">The object ID of a managed object for this call.</param>
        /// <param name="isConstructCall">'true' if this is called because of the 'new' operator in JS.</param>
        /// <param name="_this">The instance context, if any.</param>
        /// <param name="args">Any arguments from the JS call.</param>
        /// <param name="argCount">Number of arguments given in the JS call.</param>
        /// <returns></returns>
        HandleProxy* _CallBack(Int32 managedObjectID, bool isConstructCall, HandleProxy* _this, HandleProxy** args, Int32 argCount)
        {
            lock (_FunctionsByType)
            {
                // ... get a list of callback functions to execute for this callback ...

                var functions = from f in
                                    (from t in _FunctionsByType.Keys.ToArray() // (need to convert this to an array in case the callbacks modify the dictionary!)
                                     select _Engine._GetExistingObject(_FunctionsByType[t]))
                                where f != null && ((V8Function)f.Object).Callback != null
                                select ((V8Function)f.Object).Callback;

                return _CallBack(managedObjectID, isConstructCall, _this, args, argCount, functions.ToArray());
            }
        }

        // TODO: This is shared in both templates - consider putting elsewhere.
        internal static HandleProxy* _CallBack(Int32 managedObjectID, bool isConstructCall, HandleProxy* _this, HandleProxy** args, Int32 argCount, params JSFunction[] functions)
        {
            // ... wrap the arguments ...
            InternalHandle[] _args = new InternalHandle[argCount];
            int i;

            for (i = 0; i < argCount; i++) {
                HandleProxy* arg = args[i];
                _args[i] = new InternalHandle(arg, true); // (since these will be disposed immediately after, the "first" flag is not required [this also prevents it from getting passed on])
            }

            // (note: the underlying native handles for '_this' and any arguments will be disposed automatically upon return, unless the user calls 'KeepAlive()' on them)

            var result = InternalHandle.Empty;

            // ... get a handle to the native "this" object ...
            using (InternalHandle hThis = _this)
            {
                V8Engine engine = hThis.Engine;

                // ... call all function types (multiple custom derived function types are allowed, but only one of each type) ...
                foreach (var callback in functions)
                {
                    result = callback(engine, isConstructCall, hThis, _args);

                    if (!result.IsEmpty) break;
                }

                for (i = 0; i < argCount; i++)
                    _args[i].Dispose(); // (since these will be disposed immediately after, the "first" flag is not required [this also prevents it from getting passed on])

                var obj = result.Object;

                // ... make sure the user is not returning a 'V8ManagedObject' instance associated with the new object (the property interceptors will never work) ...

                if (isConstructCall && obj != null && obj is V8ManagedObject && obj.InternalHandle == hThis)
                    throw new InvalidOperationException("You've attempted to return the type '" + obj.GetType().Name
                        + "' which is of type V8ManagedObject in a construction call (using 'new' in JavaScript) to wrap the new native object given to the constructor.  The native V8 engine"
                        + " only supports interceptor hooks for objects generated from ObjectTemplate instances.  You will need to first derive/implement from V8NativeObject/IV8NativeObject"
                        + " for your custom object(s), or rewrite your object to use V8NativeObject directly instead and use the 'SetAccessor()' handle method.");
            }

            using (result.KeepAlive())
                return result;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Returns the specified V8Function object type associated with this function template.
        /// There can only ever be one native V8 function object per native V8 function template in a single native V8 JavaScript context;
        /// however, V8.NET (the managed side) does allow multiple function types per template. In this case, a single call triggers all derived types at once.
        /// The first callback to return a value terminates the cycle and any following callbacks are ignored.
        /// <para>WARNING: The returned function object will be garbage collected if you don't store the reference anywhere. If this happens, then calling 
        /// the function object in JavaScript will return "undefined".</para>
        /// </summary>
        /// <typeparam name="T">A type that implements IV8Function, or derives from V8Function.</typeparam>
        /// <param name="callback">When a new instance of type 'T' is created, it's 'Callback' property will overwritten by this value (replacing anything that may be set when it was created).
        /// It is expect to provide a callback method when using the default 'V8Function' object, but if you have a custom derivation you can set this to 'null'.</param>
        public T GetFunctionObject<T>(JSFunction callback = null) where T : V8Function, new()
        {
            if (_Engine == null)
                throw new InvalidOperationException("You must create object templates by calling one of the 'V8Engine.CreateFunctionTemplate()' overloads.");

            if (_NativeFunctionTemplateProxy == null)
                throw new InvalidOperationException("This managed function template is not initialized.");

            int funcID;
            V8Function func;

            lock (_FunctionsByType)
            {
                if (_FunctionsByType.TryGetValue(typeof(T), out funcID))
                {
                    func = _Engine._GetExistingObject(funcID) as V8Function;
                    if (func != null)
                        return (T)func;
                }
            }

            // ... get the v8 "Function" object ...

            InternalHandle hNativeFunc = V8NetProxy.GetFunction(_NativeFunctionTemplateProxy);
            // ... create a managed wrapper for the V8 "Function" object (note: functions inherit the native V8 "Object" type) ...

            func = _Engine._GetObject<T>(this, hNativeFunc, true, false); // (note: this will "connect" the native object [hNativeFunc] to a new managed V8Function wrapper, and set the prototype!)
            
            if (callback != null)
                func.Callback = callback;

            // ... get the function's prototype object, wrap it, and give it to the new function object ...
            // (note: this is a special case, because the function object auto generates the prototype object natively using an existing object template)

            using (InternalHandle funcProto = V8NetProxy.GetObjectPrototype(func._Handle))
            {
                func._Prototype.Set(funcProto);

                Engine.AddToMemorySnapshots(funcProto);
            }

            lock (_FunctionsByType)
            {
                _FunctionsByType[typeof(T)] = func.ID; // (this exists to index functions by type)
            }

            func.Initialize(false, null);

            return (T)func;
        }

        /// <summary>
        /// Returns a JavaScript V8Function object instance associated with this function template.
        /// There can only ever be ONE V8 function object per V8 function template in a single V8 JavaScript context;
        /// however, V8.NET does allow one MANAGED function type per managed template. In this case, a single call triggers all derived types at once.
        /// The first callback to return a value terminates the cycle and any following callbacks are ignored.
        /// <para>WARNING: The returned function object will be garbage collected if you don't store the reference anywhere. If this happens, then calling 
        /// the function object in JavaScript will return "undefined". This is because function object callbacks are dynamic and are only valid when
        /// the calling object is still in use.</para>
        /// </summary>
        /// <param name="callback">When a new instance of V8Function is created, it's 'Callback' property will set to the specified value.
        /// If you don't provide a callback, then calling the function in JavaScript will simply do nothing and return "undefined".</param>
        public V8Function GetFunctionObject(JSFunction callback) { return GetFunctionObject<V8Function>(callback); }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the underlying native function to create a new native object and return its handle.
        /// Use this method if you only need the native object and not a managed wrapper.
        /// </summary>
        /// <param name="args">Arguments to pass to the function to construct the new native instance.</param>
        /// <returns>A handle to the new object.</returns>
        public InternalHandle CreateNativeInstance(params InternalHandle[] args) // TODO: Parameter passing needs testing.
        {
            HandleProxy** _args = null;

            if (args.Length > 0)
            {
                _args = (HandleProxy**)Utilities.AllocPointerArray(args.Length);
                for (var i = 0; i < args.Length; i++)
                    _args[i] = args[i];
            }

            try
            {
                return (InternalHandle)V8NetProxy.CreateInstanceFromFunctionTemplate(_NativeFunctionTemplateProxy, -1, args.Length, _args);
            }
            finally
            {
                Utilities.FreeNativeMemory((IntPtr)_args);
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the underlying native function to create and return a new instance, which will be wrapped in the specified managed object type.
        /// </summary>
        /// <typeparam name="T">A managed object type to wrap the new native object handle.</typeparam>
        /// <param name="args">Arguments to pass to the function to construct the new native instance.</param>
        /// <returns>A new instance of 'T'.</returns>
        public V8ManagedObject CreateInstance<T>(params InternalHandle[] args) // TODO: Parameter passing needs testing.
            where T : V8ManagedObject, new()
        {
            HandleProxy** _args = null;

            if (args.Length > 0)
            {
                _args = (HandleProxy**)Utilities.AllocPointerArray(args.Length);
                for (var i = 0; i < args.Length; i++)
                    _args[i] = args[i];
            }

            // (note: the special case here is that the native function object will use its own template to create instances)

            T obj = _Engine._CreateManagedObject<T>(this, null);
            obj.Template = InstanceTemplate;

            try
            {
                obj._Handle.Set(V8NetProxy.CreateInstanceFromFunctionTemplate(_NativeFunctionTemplateProxy, obj.ID, args.Length, _args));
                // (note: setting '_NativeObject' also updates it's '_ManagedObject' field if necessary.

                obj.Initialize(true, args);
            }
            catch (Exception ex)
            {
                // ... something went wrong, so remove the new managed object ...
                _Engine._RemoveObjectRootableReference(obj.ID);
                throw ex;
            }
            finally
            {
                Utilities.FreeNativeMemory((IntPtr)_args);
            }

            return obj;
        }

        /// <summary>
        /// Calls the underlying native function to create and return a new instance, which will be wrapped in a 'V8ManagedObject' instance.
        /// </summary>
        /// <param name="args">Arguments to pass to the function to construct the new native instance.</param>
        /// <returns>A new instance of 'V8ManagedObject'.</returns>
        public V8ManagedObject CreateInstance(params InternalHandle[] args) // TODO: Parameter passing needs testing.
        {
            return CreateInstance<V8ManagedObject>(args);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// This is called by '{V8NativeObject}._OnNativeGCRequested()' when the managed function object is ready to be deleted.
        /// </summary>
        internal void _RemoveFunctionType(int objectID)
        {
            lock (_FunctionsByType)
            {
                var callbackTypes = _FunctionsByType.Keys.ToArray();
                for (var i = 0; i < callbackTypes.Length; i++)
                    if (_FunctionsByType[callbackTypes[i]] == objectID)
                        _FunctionsByType[callbackTypes[i]] = -1;
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the V8 'Set()' function on the underlying native function template to set properties that will exist on all function objects created from this template.
        /// </summary>
        public void SetProperty(string name, InternalHandle value, V8PropertyAttributes attributes = V8PropertyAttributes.Undefined)
        {
            if (name.IsNullOrWhiteSpace()) throw new ArgumentNullException("name (cannot be null, empty, or only whitespace)");

            V8NetProxy.SetFunctionTemplateProperty(_NativeFunctionTemplateProxy, name, value, attributes);
        }

        // --------------------------------------------------------------------------------------------------------------------
    }

    // ========================================================================================================================
}
