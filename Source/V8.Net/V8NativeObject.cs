﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

#if !(V1_1 || V2 || V3 || V3_5)
using System.Dynamic;
using System.Linq.Expressions;
#endif

namespace V8.Net
{
    // ========================================================================================================================

    /// <summary>
    /// An interface for objects wrapped by V8NativeObject instances.
    /// <para>These methods are called in proxy to the V8NativeObject's related methods ('Initialize(...)' and 'Dispose(...)').</para>
    /// The arguments passed to 'Initialize(...)' ('isConstructCall' and 'args') are the responsibility of the developer - except for the binder, which will
    /// pass in the values as expected.
    /// </summary>
    public interface IV8NativeObject
    {
        // --------------------------------------------------------------------------------------------------------------------

        ///// <summary>
        ///// Returns the ID used to track the underlying object.
        ///// </summary>
        //?public Int32 ID { get; }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Called immediately after creating an object instance and setting the V8Engine property.
        /// Derived objects should override this for construction instead of using the constructor, and be sure to call back to this base method just before exiting (not at the beginning).
        /// In the constructor, the object only exists as an empty shell.
        /// It's ok to setup non-v8 values in constructors, but be careful not to trigger any calls into the V8Engine itself.
        /// <para>Note: Because this method is virtual, it does not guarantee that 'IsInitialized' will be considered.  Implementations should check against
        /// the 'IsInitilized' property.</para>
        /// </summary>
        void Initialize(V8NativeObject owner, bool isConstructCall, params InternalHandle[] args);

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Called when there are no more references on either the managed or native side.  In such case the object is ready to
        /// be deleted from the V8.NET system.
        /// <para>You should never call this from code directly unless you need to force the release of native resources associated
        /// with a custom implementation (and if so, a custom internal flag should be kept indicating whether or not the
        /// resources have been disposed to be safe).</para>
        /// <para>You should always override/implement this if you need to dispose of any native resources in custom implementations.</para>
        /// <para>DO NOT rely on the destructor (finalizer) - some objects may survive it (due to references on the native V8 side).</para>
        /// <para>Note: This can be triggered via the worker thread.</para>
        /// </summary>
        void OnDispose();

        // --------------------------------------------------------------------------------------------------------------------
    }

    /// <summary>
    /// Represents a basic JavaScript object. This class wraps V8 functionality for operations required on any native V8 object (including managed ones).
    /// <para>This class implements 'DynamicObject' to make setting properties a bit easier.</para>
    /// </summary>
    public unsafe class V8NativeObject : Handle, IV8NativeObject, IV8Object, IDynamicMetaObjectProvider
//#if DEBUG
    , IV8DebugInfo
//#endif
    {
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// A reference to the V8Engine instance that owns this object.
        /// The default implementation for <see cref="V8NativeObject"/> is to cache and return 'base.Engine', since it inherits from 'Handle'.
        /// </summary>
        new public V8Engine Engine { get { return _Engine ?? (_Engine = _Handle.Engine); } }
        internal V8Engine _Engine;
        public bool IsLocked;

        public void SetReadyToDisposal()
        {
            IsLocked = false;
        }

//#if DEBUG
        public V8EntityID SelfID
        {
            get {
                return new V8EntityID(_Handle.HandleID, _Handle.ObjectID);
            } 

            set {} 
        }

        public V8EntityID ParentID
        {
            get {return null;} 
        }

        public List<V8EntityID> ChildIDs
        {
            get {
                var res = new List<V8EntityID>();
                if (!_Prototype.IsEmpty) {
                    res.Add(new V8EntityID(_Prototype.HandleID, _Prototype.ObjectID));
                }
                return res;
            } 
        }

        public string Summary
        {
            get {return "";}
        }
//#endif

        new public V8NativeObject Object { get { return this; } }

        /// <summary>
        ///     This is updated to hold a reference to the property value getter callback when
        ///     <see cref="SetAccessor(string, GetterAccessor, SetterAccessor, V8PropertyAttributes, V8AccessControl)"/> is called.
        ///     Without a rooted reference the delegate will get garbage collected causing callbacks from the native side will fail.
        /// </summary>
        /// <value> The getter. </value>
        public GetterAccessor Getter { get; private set; }
        NativeGetterAccessor _Getter;
        /// <summary>
        ///     This is updated to hold a reference to the property value setter callback when
        ///     <see cref="SetAccessor(string, GetterAccessor, SetterAccessor, V8PropertyAttributes, V8AccessControl)"/> is called.
        ///     Without a rooted reference the delegate will get garbage collected causing callbacks from the native side will fail.
        /// </summary>
        /// <value> The getter. </value>
        public SetterAccessor Setter { get; private set; }
        NativeSetterAccessor _Setter;

        /// <summary>
        /// The V8.NET ObjectTemplate or FunctionTemplate instance associated with this object, if any, or null if this object was not created using a V8.NET template.
        /// </summary>
        public ITemplate Template
        {
            get { return _Template; }
            internal set
            {
                if (_Template != null) ((ITemplateInternal)_Template)._ReferenceCount--;
                _Template = value;
                if (_Template != null) ((ITemplateInternal)_Template)._ReferenceCount++;
            }
        }
        ITemplate _Template;

        /// <summary>
        /// The V8.NET managed object ID used to track this object instance on both the native and managed sides.
        /// </summary>
        public Int32 ID
        {
            get { var id = _Handle.ObjectID; return id < 0 ? (_ID ?? id) : id; } // (this attempts to return the underlying managed object ID of the handle proxy, or the local ID if -1)
            set
            {
                if ((_ID ?? -1) >= 0)
                    throw new InvalidOperationException("Cannot set the ID of a V8NativeObject that is created from within V8Engine.");
                if (value >= 0)
                    throw new InvalidOperationException("Invalid object ID: Values >= 0 are reserved for V8NativeObject instances created from within V8Engine.");
                _Handle.ObjectID = value; // (just to make sure, as it may be -1 [default value on native side])
                _ID = value;
            }
        }
        internal Int32? _ID;

        /// <summary>
        /// Another object of the same interface to direct actions to (such as 'Initialize()').
        /// If the generic type 'V8NativeObject&lt;T>' is used, then this is set to an instance of "T", otherwise this is set to "this" instance.
        /// </summary>
        public IV8NativeObject Proxy { get { return _Proxy; } }
        internal IV8NativeObject _Proxy; // (Note: MUST NEVER BE NULL)

        /// <summary>
        /// True if this object was initialized and is ready for use.
        /// </summary>
        public bool IsInitilized { get; internal set; }

        /// <summary>
        /// A reference to the managed object handle that wraps the native V8 handle for this managed object.
        /// The default implementation for 'V8NativeObject' is to return itself, since it inherits from 'Handle'.
        /// Setting this property will call the inherited 'Set()' method to replace the handle associated with this object instance (this should never be done on
        /// objects created from templates ('V8ManagedObject' objects), otherwise callbacks from JavaScript to the managed side will not act as expected, if at all).
        /// </summary>
        public override InternalHandle InternalHandle
        {
            get { return _Handle; }
            set
            {
                if (value.ObjectID >= 0 && value.Object != this)
                    throw new InvalidOperationException("Another managed object is already bound to this handle.");

                if (!_Handle.IsEmpty && _Handle.ObjectID >= 0)
                    throw new InvalidOperationException("Cannot replace a the handle of an object created by the V8Engine once it has been set."); // (IDs < 0 are not tracked in the V8.NET's object list)
                else
                {
                    if (!value.IsEmpty && !value.IsObjectType)
                        throw new InvalidCastException(string.Format(InternalHandle._VALUE_NOT_AN_OBJECT_ERRORMSG, value));

                    _Handle._Object = null; // (REQUIRED to unlock the handle)
                    _Handle.Dispose();
                    _Handle = value;

                    if (!_Handle.IsEmpty)
                    {
                        _Handle._Object = this;
                        _ID = _Handle._HandleProxy->_ObjectID; // (MUST CHANGE THIS FIRST before setting '_Handle.ObjectID', else the '_Object' reference will be nullified)
                        _Handle.ObjectID = _Handle._HandleProxy->_ObjectID; // (setting 'ObjectID' anyway, which may cause other updates)
                    }
                }
            }
        }

#if !(V1_1 || V2 || V3 || V3_5)
        /// <summary>
        /// Returns a "dynamic" reference to this object (which is simply the handle instance, which has dynamic support).
        /// </summary>
        public virtual dynamic AsDynamic { get { return _Handle; } }
#endif

        /// <summary>
        /// The prototype of the object (every JavaScript object implicitly has a prototype).
        /// </summary>
        public InternalHandle Prototype
        {
            get
            {
                if (_Prototype.IsEmpty && _Handle.IsObjectType)
                {
                    // ... the prototype is not yet set, so get the prototype and wrap it ...
                    _Prototype = _Handle.GetPrototype();
                }

                return _Prototype;
            }
        }
        internal InternalHandle _Prototype;

        /// <summary>
        /// Used internally to quickly determine when an instance represents a binder object type, or static type binder function (faster than reflection!).
        /// </summary>
        public BindingMode BindingType { get { return _BindingMode; } }
        internal BindingMode _BindingMode;

        // --------------------------------------------------------------------------------------------------------------------

        public virtual InternalHandle this[string propertyName]
        {
            get
            {
                return _Handle.GetProperty(propertyName);
            }
            set
            {
                _Handle.SetProperty(propertyName, value);
            }
        }

        public virtual InternalHandle this[int index]
        {
            get
            {
                return _Handle.GetProperty(index);
            }
            set
            {
                _Handle.SetProperty(index, value);
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

#if TRACE
        /// <summary>
        /// Holds the call stack responsible for creating this object (available only with TRACE defined).
        /// </summary>
        string _CreationStack;
#endif


        public V8NativeObject()
        {
#if  TRACE
            _CreationStack = Environment.StackTrace;
#endif
            _Proxy = this;
            IsLocked = true;
        }

        public V8NativeObject(IV8NativeObject proxy)
        {
#if TRACE
            _CreationStack = Environment.StackTrace;
#endif
            _Proxy = proxy ?? this;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Called immediately after creating an object instance and setting the V8Engine property.
        /// Derived objects should override this for construction instead of using the constructor, and be sure to call back to this base method just before exiting (not at the beginning).
        /// In the constructor, the object only exists as an empty shell.
        /// It's ok to setup non-v8 values in constructors, but be careful not to trigger any calls into the V8Engine itself.
        /// <para>Note: Because this method is virtual, it does not guarantee that 'IsInitialized' will be considered.  Implementations should check against
        /// the 'IsInitilized' property.</para>
        /// </summary>
        public virtual InternalHandle Initialize(bool isConstructCall, params InternalHandle[] args)
        {
            if (_Proxy != this && !IsInitilized)
                _Proxy.Initialize(this, isConstructCall, args);

            IsInitilized = true;

            return _Handle;
        }

        /// <summary>
        /// (Exists only to support the 'IV8NativeInterface' interface and should not be called directly - call 'Initialize(isConstructCall, args)' instead.)
        /// </summary>
        public void Initialize(V8NativeObject owner, bool isConstructCall, params InternalHandle[] args)
        {
            if (!IsInitilized)
                Initialize(isConstructCall, args);
        }

        public override bool CanDispose
        {
            get { return _Handle.IsEmpty || !_Handle.IsObjectType; }
        }

        public override void Dispose()
        {
            DisposeObject(false);
        }

        /// <summary>
        /// Called when there are no more references on either the managed or native side.  In such case the object is ready to
        /// be deleted from the V8.NET system.
        /// <para>You should never call this from code directly unless you need to force the release of native resources associated
        /// with a custom implementation (and if so, a custom internal flag should be kept indicating whether or not the
        /// resources have been disposed to be safe).</para>
        /// <para>You should always override/implement this if you need to dispose of any native resources in custom implementations.</para>
        /// <para>DO NOT rely on the destructor (finalizer) - some objects may survive it (due to references on the native V8 side).</para>
        /// <para>Note: This can be triggered via the worker thread.</para>
        /// </summary>
        public virtual void OnDispose()
        {
            if (_Proxy != this)
                _Proxy.OnDispose();
        }

        /// <summary>
        ///     Returns true if disposed, and false if already disposed.
        /// </summary>
        public bool DisposeObject(bool fromInternalHandle = false) // WARNING: The worker thread may cause a V8 GC callback in its own thread!
        {
            if (!_Handle.IsEmpty)
            {
                //? _Handle.IsDisposing = true;
                var engine = Engine;

                //// ... remove this object from the abandoned queue ...

                //lock (engine._AbandondObjects)
                //{
                //    LinkedListNode<IV8Disposable> node;
                //    if (engine._AbandondObjectsIndex.TryGetValue(this, out node))
                //    {
                //        engine._AbandondObjects.Remove(node);
                //        engine._AbandondObjectsIndex.Remove(this);
                //    }
                //}

                // ... notify any custom dispose methods to clean up ...

                try
                {
                    OnDispose();
                }
                finally
                {
                }

                // ... if this belongs to a function template, then this is a V8Function object, so remove it from the template's type list ...

                if (Template is FunctionTemplate)
                    ((FunctionTemplate)Template)._RemoveFunctionType(ID);// (make sure to remove the function references from the template instance)

                // ... clear any registered accessors ...

                Getter = null;
                _Getter = null;
                Setter = null;
                _Setter = null;

                _Prototype.Dispose();
                // ... reset and dispose the handle ...

                //_Handle.ObjectID = -1; // (resets the object ID on the native side [though this happens anyhow once cached], which also causes the reference to clear)
                // (MUST clear the object ID, else the handle will not get disposed [because '{Handle}.IsLocked' will return false])
                if (!fromInternalHandle) {
                    InternalHandle h = _Handle;
                    _Handle = InternalHandle.Empty;
                    h.ForceDispose(false, true);
                }

                var objectID = _ID != null ? _ID.Value : -1;

                Template = null; // (note: this decrements a template counter, allowing the template object to be finally allowed to dispose)
                _ID = null; // (also allows the GC finalizer to collect the object)
        
                if (objectID >= 0)
                    engine._RemoveObjectRootableReference(objectID);

                GC.SuppressFinalize(this); // added KSI
                return true;
            }

            return false;
        }

        // --------------------------------------------------------------------------------------------------------------------

        public static implicit operator InternalHandle(V8NativeObject obj) { return obj != null ? obj._Handle : InternalHandle.Empty; }
        public static implicit operator HandleProxy* (V8NativeObject obj) { return obj != null ? obj._Handle._HandleProxy : null; }

        // --------------------------------------------------------------------------------------------------------------------

        public override string ToString()
        {
            var objText = _Proxy.GetType().Name;
            var disposeText = _Handle.IsDisposed ? "Yes" : "No";
            return objText + " (ID: " + ID + " / Value: '" + _Handle + "' / Is Disposed?: " + disposeText + ")";
        }

        // --------------------------------------------------------------------------------------------------------------------

#if !(V1_1 || V2 || V3 || V3_5)
        new public DynamicMetaObject GetMetaObject(Expression parameter)
        {
            return new DynamicHandle(this, parameter);
        }
#endif

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the V8 'Set()' function on the underlying native object.
        /// Returns true if successful.
        /// </summary>
        /// <param name="attributes">Flags that describe the property behavior.  They must be 'OR'd together as needed.</param>
        public virtual bool SetProperty(string name, InternalHandle value, V8PropertyAttributes attributes = V8PropertyAttributes.None)
        {
            return _Handle.SetProperty(name, value, attributes);
        }

        /// <summary>
        /// Calls the V8 'Set()' function on the underlying native object.
        /// Returns true if successful.
        /// </summary>
        /// <param name="index"> Zero-based index to set. </param>
        /// <param name="value"> The value to set. </param>
        /// <param name="attributes">
        ///     (Optional) Flags that describe the property behavior.  They must be 'OR'd together as needed.
        ///     <para>Warning: V8 does not support setting attributes using numerical indexes.  If you set an attribute, the given
        ///     value is converted to a string, and a named property setter will be used instead. </para>
        /// </param>
        public virtual bool SetProperty(Int32 index, InternalHandle value, V8PropertyAttributes attributes = V8PropertyAttributes.Undefined)
        {
            return _Handle.SetProperty(index, value, attributes);
        }

        /// <summary>
        /// Sets a property to a given object. If the object is not V8.NET related, then the system will attempt to bind the instance and all public members to
        /// the specified property name.
        /// Returns true if successful.
        /// </summary>
        /// <param name="name">The property name.</param>
        /// <param name="obj">Some value or object instance. 'Engine.CreateValue()' will be used to convert value types.</param>
        /// <param name="className">A custom in-script function name for the specified object type, or 'null' to use either the type name as is (the default) or any existing 'ScriptObject' attribute name.</param>
        /// <param name="recursive">For object instances, if true, then object reference members are included, otherwise only the object itself is bound and returned.
        /// For security reasons, public members that point to object instances will be ignored. This must be true to included those as well, effectively allowing
        /// in-script traversal of the object reference tree (so make sure this doesn't expose sensitive methods/properties/fields).</param>
        /// <param name="memberSecurity">For object instances, these are default flags that describe JavaScript properties for all object instance members that
        /// don't have any 'ScriptMember' attribute.  The flags should be 'OR'd together as needed.</param>
        public virtual bool SetProperty(string name, object obj, string className = null, bool? recursive = null, ScriptMemberSecurity? memberSecurity = null)
        {
            return _Handle.SetProperty(name, obj, className, recursive, memberSecurity);
        }

        /// <summary>
        /// Binds a 'V8Function' object to the specified type and associates the type name (or custom script name) with the underlying object.
        /// Returns true if successful.
        /// </summary>
        /// <param name="type">The type to wrap.</param>
        /// <param name="propertyAttributes">Flags that describe the property behavior.  They must be 'OR'd together as needed.</param>
        /// <param name="className">A custom in-script function name for the specified type, or 'null' to use either the type name as is (the default) or any existing 'ScriptObject' attribute name.</param>
        /// <param name="recursive">For object types, if true, then object reference members are included, otherwise only the object itself is bound and returned.
        /// For security reasons, public members that point to object instances will be ignored. This must be true to included those as well, effectively allowing
        /// in-script traversal of the object reference tree (so make sure this doesn't expose sensitive methods/properties/fields).</param>
        /// <param name="memberSecurity">For object instances, these are default flags that describe JavaScript properties for all object instance members that
        /// don't have any 'ScriptMember' attribute.  The flags should be 'OR'd together as needed.</param>
        public virtual bool SetProperty(Type type, V8PropertyAttributes propertyAttributes = V8PropertyAttributes.None, string className = null, bool? recursive = null, ScriptMemberSecurity? memberSecurity = null, bool addToLastMemorySnapshotBefore = false)
        {
            return _Handle.SetProperty(type, propertyAttributes, className, recursive, memberSecurity, addToLastMemorySnapshotBefore);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the V8 'Get()' function on the underlying native object.
        /// If the property doesn't exist, the 'IsUndefined' property will be true.
        /// </summary>
        public virtual InternalHandle GetProperty(string name)
        {
            return _Handle.GetProperty(name);
        }

        /// <summary>
        /// Calls the V8 'Get()' function on the underlying native object.
        /// If the property doesn't exist, the 'IsUndefined' property will be true.
        /// </summary>
        public virtual InternalHandle GetProperty(Int32 index)
        {
            return _Handle.GetProperty(index);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls the V8 'Delete()' function on the underlying native object.
        /// Returns true if the property was deleted.
        /// </summary>
        public virtual bool DeleteProperty(string name)
        {
            return _Handle.GetProperty(name);
        }

        /// <summary>
        /// Calls the V8 'Delete()' function on the underlying native object.
        /// Returns true if the property was deleted.
        /// </summary>
        public virtual bool DeleteProperty(Int32 index)
        {
            return _Handle.GetProperty(index);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        ///     Calls the V8 'SetAccessor()' function on the underlying native object to create a property that is controlled by
        ///     "getter" and "setter" callbacks.
        ///     <para>WARNING: If you try to set managed accessors on a native-ONLY object (as in, this handle does not yet have a
        ///     managed-side object associated with it) then
        ///     <see cref="V8Engine.CreateObject(InternalHandle, bool)"/> will be called to create a wrapper object so the
        ///     accessor delegates will not get garbage collected, causing errors. You can optionally take control of this yourself
        ///     and call one of the 'CreateObject()' methods on <see cref="V8Engine"/>.</para>
        /// </summary>
        /// <param name="name"> The property name. </param>
        /// <param name="getter">
        ///     The property getter delegate that returns a value when the property is accessed within JavaScript.
        /// </param>
        /// <param name="setter">
        ///     The property setter delegate that sets AND returns a value when the property is accessed within JavaScript.
        /// </param>
        /// <param name="attributes"> (Optional) The attributes to assign to the property. </param>
        /// <param name="access"> (Optional) The access security on the property. </param>
        /// <seealso cref="M:V8.Net.IV8Object.SetAccessor(string,GetterAccessor,SetterAccessor,V8PropertyAttributes,V8AccessControl)"/>
        public virtual void SetAccessor(string name, GetterAccessor getter, SetterAccessor setter,
            V8PropertyAttributes attributes = V8PropertyAttributes.None, V8AccessControl access = V8AccessControl.Default)
        {
            attributes = _CreateAccessorProxies(Engine, name, getter, setter, attributes, access, ref _Getter, ref _Setter);

            Getter = getter;
            Setter = setter;

            V8NetProxy.SetObjectAccessor(this, ID, name, _Getter, _Setter, access, attributes);
        }

        static internal V8PropertyAttributes _CreateAccessorProxies(V8Engine engine, string name, GetterAccessor getter, SetterAccessor setter, V8PropertyAttributes attributes, V8AccessControl access,
            ref NativeGetterAccessor _Getter, ref NativeSetterAccessor _Setter)
        {
            if (name.IsNullOrWhiteSpace())
                throw new ArgumentNullException(nameof(name), "Cannot be null, empty, or only whitespace.");
            if (attributes == V8PropertyAttributes.Undefined)
                attributes = V8PropertyAttributes.None;
            if (attributes < 0) throw new InvalidOperationException("'attributes' has an invalid value.");
            if (access < 0) throw new InvalidOperationException("'access' has an invalid value.");

            if (getter != null && _Getter == null)
                _Getter = (_this, _name) => // (only need to set this once on first use)
                {
                    try
                    {
                        return getter != null ? getter(_this, _name) : null;
                    }
                    catch (Exception ex)
                    {
                        return engine.CreateError(Exceptions.GetFullErrorMessage(ex), JSValueType.ExecutionError);
                    }
                };

            if (setter != null && _Setter == null)
                _Setter = (_this, _name, _val) => // (only need to set this once on first use)
                {
                    try
                    {
                        return setter != null ? setter(_this, _name, _val) : null;
                    }
                    catch (Exception ex)
                    {
                        return engine.CreateError(Exceptions.GetFullErrorMessage(ex), JSValueType.ExecutionError);
                    }
                };

            return attributes;
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Returns a list of all property names for this object (including all objects in the prototype chain).
        /// </summary>
        public virtual string[] GetPropertyNames()
        {
            return _Handle.GetPropertyNames();
        }

        /// <summary>
        /// Returns a list of all property names for this object (excluding the prototype chain).
        /// </summary>
        public virtual string[] GetOwnPropertyNames()
        {
            return _Handle.GetOwnPropertyNames();
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Get the attribute flags for a property of this object.
        /// If a property doesn't exist, then 'V8PropertyAttributes.None' is returned
        /// (Note: only V8 returns 'None'. The value 'Undefined' has an internal proxy meaning for property interception).</para>
        /// </summary>
        public virtual V8PropertyAttributes GetPropertyAttributes(string name)
        {
            return _Handle.GetPropertyAttributes(name);
        }

        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Calls an object property with a given name on a specified object as a function and returns the result.
        /// The '_this' property is the "this" object within the function when called.
        /// </summary>
        public virtual InternalHandle Call(string functionName, InternalHandle _this, params InternalHandle[] args)
        {
            return _Handle.Call(functionName, _this, args);
        }

        /// <summary>
        /// Calls an object property with a given name on a specified object as a function and returns the result.
        /// </summary>
        public virtual InternalHandle StaticCall(string functionName, params InternalHandle[] args)
        {
            return _Handle.StaticCall(functionName, args);
        }

        /// <summary>
        /// Calls the underlying object as a function.
        /// The '_this' parameter is the "this" reference within the function when called.
        /// </summary>
        public virtual InternalHandle Call(InternalHandle _this, params InternalHandle[] args)
        {
            return _Handle.Call(_this, args);
        }

        /// <summary>
        /// Calls the underlying object as a function.
        /// The 'this' property will not be specified, which will default to the global scope as expected.
        /// </summary>
        public virtual InternalHandle StaticCall(params InternalHandle[] args)
        {
            return _Handle.StaticCall(args);
        }

        // --------------------------------------------------------------------------------------------------------------------
    }

    // ========================================================================================================================

    /// <summary>
    /// This generic version of 'V8NativeObject' allows injecting your own class by implementing the 'IV8NativeObject' interface.
    /// </summary>
    /// <typeparam name="T">Your own class, which implements the 'IV8NativeObject' interface.  Don't use the generic version if you are able to inherit from 'V8NativeObject' instead.</typeparam>
    public unsafe class V8NativeObject<T> : V8NativeObject
        where T : IV8NativeObject, new()
    {
        // --------------------------------------------------------------------------------------------------------------------

        public V8NativeObject()
            : base(new T())
        {
        }

        // --------------------------------------------------------------------------------------------------------------------
    }

    // ========================================================================================================================
}
