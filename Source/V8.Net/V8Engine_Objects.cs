using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

#if !(V1_1 || V2 || V3 || V3_5)
using System.Dynamic;
#endif

namespace V8.Net
{
    // ========================================================================================================================

    public unsafe class WeakReferenceStub
    {
        public object Target;

        public WeakReferenceStub(object target, bool trackResurrection = true)
        {
            Target = target;
        }

    }


    public unsafe class CountedReference : WeakReferenceStub
    {
        /// <summary> Allows overriding the weak reference by rooting the target object to this entry. </summary>
        private int _RefCount;
        internal const int UndefinedRefCount = -999;


        public CountedReference(Handle target, bool trackResurrection = true) : base(target, trackResurrection)
        {
            Reinitialize(target);
        }

        public void Reinitialize(Handle target)
        {
            Reset(target);
            //Inc();
        }


        public void Reset(Handle target = null)
        {
#if DEBUG
            if (IsLocked) {
                InternalHandle h = ((Handle)Target)._;
                throw new InvalidOperationException($"Can't reinitialize CountedReference: RefCount={_RefCount}, Target.HandleID={h.HandleID}, Target.ValueType={h.ValueType}.");
            }
#endif

            if (target == null && Target != null) {
                GC.SuppressFinalize(Target);
            }
            Target = target;
            _RefCount = target != null ? 0 : UndefinedRefCount;
        }

        public bool IsToBeKeptAlive {
            get {
                return false;
            }
            set {
            }
        }

        public int RefCount {
            get {
                return _RefCount;
            }
        }

        public bool IsRefCountPresent {
            get {
                return _RefCount != UndefinedRefCount;
            }
        }

        public bool IsLocked {
            get 
            {
                return _RefCount > 0;
            }
        }

        public void Inc()
        {
#if DEBUG
            if (IsRefCountPresent && _RefCount < 0) {
                InternalHandle h = ((Handle)Target)._;
                throw new InvalidOperationException($"Can't Inc CountedReference: RefCount={_RefCount}, Target.HandleID={h.HandleID}, Target.ValueType={h.ValueType}.");
            }
#endif
            if (_RefCount >= 0) {
                _RefCount += 1;
            }
        }

        public void Dec()
        {
#if DEBUG
            if (IsRefCountPresent && _RefCount <= 0) {
                InternalHandle h = ((Handle)Target)._;
                throw new InvalidOperationException($"Can't Dec CountedReference: RefCount={_RefCount}, Target.HandleID={h.HandleID}, Target.ValueType={h.ValueType}.");
            }
#endif
            if (_RefCount > 0) {
                _RefCount -= 1;
            }
        }
    }



    /// <summary> Allows overriding the weak reference by rooting the target object to this entry. </summary>
    public unsafe class RootableReference : WeakReferenceStub
    {
        /// <summary> Allows overriding the weak reference by rooting the target object to this entry. </summary>
        public InternalHandle RootedHandle = InternalHandle.Empty;

        public RootableReference(IV8NativeObject target, bool trackResurrection = true) : base(target, trackResurrection)
        {
        }

        ~RootableReference()
        {
            RootedHandle.CountedRef?.Dec();
        }
    }

    public unsafe partial class V8Engine
    {
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Holds an index of all the created objects.
        /// </summary>
        internal readonly IndexedObjectList<RootableReference> _Objects = new IndexedObjectList<RootableReference>();
        internal readonly ReaderWriterLock _ObjectsLocker = new ReaderWriterLock();

        // --------------------------------------------------------------------------------------------------------------------

        ///// <summary>
        ///// A list of objects that no longer have any CLR references.  These objects will sit indefinitely until the V8 GC says
        ///// to removed them.
        ///// </summary>
        //?internal readonly IEnumerable<IV8NativeObject> _AbandondObjects; // (objects are abandoned when {ObservableWeakReference}.IsGCReady becomes true)

        // --------------------------------------------------------------------------------------------------------------------

        internal RootableReference _GetObjectCountedReference(int objectID) // (performs the lookup in a lock block)
        {
            using (_ObjectsLocker.ReadLock()) { 
                return _Objects[objectID];  // (Note: if index is outside bounds, then null is returned.)
            }
        }

        internal V8NativeObject _GetExistingObject(int objectID) // (performs the object lookup in a lock block without causing a GC reset)
        {
            if (objectID < 0)
                return null;

            using (_ObjectsLocker.ReadLock()) { 
                var rootableRef = _Objects[objectID]; 
                return (V8NativeObject)rootableRef?.Target; 
            }
        }

        internal void _RemoveObjectRootableReference(int objectID) // (performs the removal in a lock block)
        {
            if (objectID < 0)
                return;

            if (_UnrootObject(objectID)) { 
                using (_ObjectsLocker.WriteLock()) { 
    #if DEBUG
                    // RootedHandle is to be empty here
                    var rootableRef = _Objects[objectID]; 
                    if (rootableRef != null && !rootableRef.RootedHandle.IsEmpty) {
                        throw new InvalidOperationException($"Attempt to remove rooted object: some Inc have been missed or extra Dec has been peformed: objectID={objectID}");
                    }
    #endif
                    _Objects.Remove(objectID);
                }
            } 
        }

        public bool IsObjectRooted(int objectID) // (looks up the object and attempts to make it unrooted)
        {
            if (objectID < 0)
                return false;

            using (_ObjectsLocker.ReadLock()) { 
                var rootableRef = _Objects[objectID]; 
                return !(rootableRef?.RootedHandle.IsEmpty ?? true); 
            }
        }

        internal bool _MakeObjectRooted(int objectID, object obj, InternalHandle h) // (looks up the object and attempts to make it rooted)
        {
            if (objectID < 0)
                return false;

            using (_ObjectsLocker.ReadLock()) { 
                var rootableRef = _Objects[objectID]; 
                if (rootableRef != null && rootableRef.RootedHandle.IsEmpty) { 
                    rootableRef.RootedHandle = new InternalHandle(ref h, true);
                } 
                else {
                    return false; 
                }
            }
            return true; 
        }

        internal bool _UnrootObject(int objectID) // (looks up the object and attempts to make it unrooted)
        {
            if (objectID < 0)
                return false;

            var h = InternalHandle.Empty;
            using (_ObjectsLocker.ReadLock()) { 
                var rootableRef = _Objects[objectID]; 
                if (rootableRef != null) { 
                    if (!rootableRef.RootedHandle.IsEmpty) { 
                        h = rootableRef.RootedHandle;
                        rootableRef.RootedHandle = InternalHandle.Empty;
                    }
                    else {
                        return true;
                    }
                } 
                else {
                    return false; 
                }
            }
            if (!h.IsEmpty) {
                return h.TryDispose(); 
            }
            else {
                return true;
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

        void _Initialize_ObjectTemplate()
        {
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Creates an uninitialized managed object ONLY (does not attempt to associate it with a JavaScript object, regardless of the supplied handle).
        /// <para>Warning: The managed wrapper is not yet initialized.  When returning the new managed object to the user, make sure to call
        /// '_ObjectInfo.Initialize()' first. Note however that new objects should only be initialized AFTER setup is completed so the users
        /// (developers) can write initialization code on completed objects (see source as example for 'FunctionTemplate.GetFunctionObject()').</para>
        /// </summary>
        /// <typeparam name="T">The wrapper type to create (such as V8ManagedObject).</typeparam>
        /// <param name="template">The managed template reference that owns the native object, if applicable.</param>
        /// <param name="handle">The handle to the native V8 object.</param>
        /// <param name="connectNativeObject">If true (the default), then a native function is called to associate the native V8 object with the new managed object.
        /// Set this to false if native V8 objects will be associated manually for special cases.  This parameter is ignored if no handle is given (hNObj == null).</param>
        internal T _CreateManagedObject<T>(ITemplate template, InternalHandle handle, bool connectNativeObject = true)
                where T : V8NativeObject, new()
        {
            T newObject;

            if (typeof(V8ManagedObject).IsAssignableFrom(typeof(T)) && template == null)
                throw new InvalidOperationException("You've attempted to create the type '" + typeof(T).Name + "' which implements IV8ManagedObject without a template (ObjectTemplate). The native V8 engine only supports interceptor hooks for objects generated from object templates.  At the very least, you can derive from 'V8NativeObject' and use the 'SetAccessor()' method.");

            if (!handle.IsUndefined)
                if (!handle.IsObjectType)
                    throw new InvalidOperationException("The specified handle does not represent an native V8 object.");
                else
                    if (connectNativeObject && handle.HasObject)
                    throw new InvalidOperationException("Cannot create a managed object for this handle when one already exists. Existing objects will not be returned by 'Create???' methods to prevent initializing more than once.");

            handle._Object = newObject = new T();  // (set the object reference on handle now so the GC doesn't take it later)
            newObject._Engine = this;
            newObject.Template = template;
            newObject._Handle = handle;

            using (_ObjectsLocker.WriteLock()) // (need a lock because of the worker thread)
            {
                var objID = _Objects.Add(new RootableReference(newObject));
                newObject._ID = objID;
                newObject._Handle.ObjectID = objID;
            }

            if (!handle.IsUndefined)
            {
                if (connectNativeObject)
                {
                    try
                    {
                        void* templateProxy = (template is ObjectTemplate) ? (void*)((ObjectTemplate)template)._NativeObjectTemplateProxy :
                            (template is FunctionTemplate) ? (void*)((FunctionTemplate)template)._NativeFunctionTemplateProxy : null;

                        V8NetProxy.ConnectObject(handle, newObject.ID, templateProxy);

                        /* The V8 object will have an associated internal field set to the index of the created managed object above for quick lookup.  This index is used
                         * to locate the associated managed object when a call-back occurs. The lookup is a fast O(1) operation using the custom 'IndexedObjectList' manager.
                         */
                    }
                    catch (Exception ex)
                    {
                        // ... something went wrong, so remove the new managed object ...
                        _RemoveObjectRootableReference(newObject.ID);
                        handle.ObjectID = -1; // (existing ID no longer valid)
                        throw ex;
                    }
                }
            }

            return newObject;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Gets the managed object that wraps the native V8 object for the specific handle.
        /// <para>Warning: You MUST pass a handle for objects only created from this V8Engine instance, otherwise you may get errors, or a wrong object (without error).</para>
        /// </summary>
        /// <typeparam name="T">You can derive your own object from V8NativeObject, or implement IV8NativeObject yourself.
        /// In either case, you can specify the type here to have it created for new object handles.</typeparam>
        /// <param name="handle">A handle to a native object that contains a valid managed object ID.</param>
        /// <param name="createIfNotFound">If true, then an IV8NativeObject of type 'T' will be created if an existing IV8NativeObject object cannot be found, otherwise 'null' is returned.</param>
        /// <param name="initializeOnCreate">If true (default) then then 'IV8NativeObject.Initialize()' is called on the created wrapper.</param>
        public T GetObject<T>(InternalHandle handle, bool createIfNotFound = true, bool initializeOnCreate = true)
            where T : V8NativeObject, new()
        {
            return _GetObject<T>(null, handle, createIfNotFound, initializeOnCreate);
        }

        /// <summary>
        /// Returns a 'V8NativeObject' or 'V8Function' object based on the handle.
        /// <see cref="GetObject&lt;T&gt;"/>
        /// </summary>
        public V8NativeObject GetObject(InternalHandle handle, bool createIfNotFound = true, bool initializeOnCreate = true)
        {
            if (handle.IsFunction)
                return GetObject<V8Function>(handle, createIfNotFound, initializeOnCreate);
            else
                return GetObject<V8NativeObject>(handle, createIfNotFound, initializeOnCreate);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Same as "Get_RemoveObjectRootableReferenceally for getting objects that are associated with templates (such as getting function prototype objects).
        /// </summary>
        internal T _GetObject<T>(ITemplate template, InternalHandle handle, bool createIfNotFound = true, bool initializeOnCreate = true, bool connectNativeObject = true)
            where T : V8NativeObject, new()
        {
            if (handle.IsEmpty)
                return null;

            if (handle.Engine != this)
                throw new InvalidOperationException("The specified handle was not generated from this V8Engine instance.");

            var obj = (T)_GetExistingObject(handle.ObjectID); // (if out of bounds or invalid, this will simply return null)

            if (obj != null)
            {
                if (!typeof(T).IsAssignableFrom(obj.GetType()))
                    throw new InvalidCastException("The existing object of type '" + obj.GetType().Name + "' cannot be converted to type '" + typeof(T).Name + "'.");
            }
            else if (createIfNotFound)
            {
                handle.ObjectID = -1; // (managed object doesn't exist [perhaps GC'd], so reset the ID)
                obj = _CreateObject<T>(template, handle, initializeOnCreate, connectNativeObject);
            }

            return obj;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Returns an object based on its ID (an object ID is simply an index value, so the lookup is fast, but it does not protect the object from
        /// garbage collection).
        /// <para>Note: If the ID is invalid, or the managed object has been garbage collected, then this will return null (no errors will occur).</para>
        /// <para>WARNING: Do not rely on this method unless you are sure the managed object is persisted. It's very possible for an object to be deleted and a
        /// new object put in the same place as identified by the same ID value. As long as you keep a reference/handle, or perform no other V8.NET actions
        /// between the time you read an object's ID, and the time this method is called, then you can safely use this method.</para>
        /// </summary>
        public V8NativeObject GetObjectByID(int objectID)
        {
            return _GetExistingObject(objectID) as V8NativeObject;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Returns all the objects using a filter expression. If no expression is given, all objects will be included.
        /// <para>Warning: This method enumerates using 'yield return' while keeping a read lock on the internal V8NativeObject 
        /// CountedReference collection. It is recommended to dump the results to an array or list if enumeration will be deferred
        /// at any point.</para>
        /// </summary>
        public IEnumerable<V8NativeObject> GetObjects(Func<V8NativeObject, bool> filter = null)
        {
            ReaderLock readerLock = new ReaderLock();

            try
            {
                if (!_ObjectsLocker.IsReaderLockHeld)
                    readerLock = _ObjectsLocker.ReadLock();

                for (var i = _Objects.Count - 1; i >= 0; --i) // (just in case items get added [whish should never happen!])
                {
                    var cref = _Objects[i];
                    if (cref != null)
                    {
                        var obj = cref.Target as V8NativeObject;
                        if (obj != null && (filter == null || filter(obj)))
                            yield return obj;
                    }
                }
            }
            finally
            {
                readerLock.Dispose();
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
    }

    // ========================================================================================================================
}
