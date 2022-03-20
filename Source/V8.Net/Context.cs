using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Runtime.InteropServices;
using System.Text;
using System.Dynamic;
using System.Threading;

namespace V8.Net
{
// ========================================================================================================================

    //public unsafe partial class V8Engine
    //{
    //x    internal readonly Dictionary<Int32, Dictionary<string, Delegate>> _Accessors = new Dictionary<Int32, Dictionary<string, Delegate>>();

    //    /// <summary>
    //    /// This is required in order prevent accessor delegates from getting garbage collected when used with P/Invoke related callbacks (a process called "thunking").
    //    /// </summary>
    //    /// <typeparam name="T">The type of delegate ('d') to store and return.</typeparam>
    //    /// <param name="key">A native pointer (usually a proxy object) to associated the delegate to.</param>
    //    /// <param name="d">The delegate to keep a strong reference to (expected to be of type 'T').</param>
    //    /// <returns>The same delegate passed in, cast to type of 'T'.</returns>
    //x    internal T _StoreAccessor<T>(Int32 id, string propertyName, T d) where T : class
    //    {
    //        Dictionary<string, Delegate> delegates;
    //        if (!_Accessors.TryGetValue(id, out delegates))
    //            _Accessors[id] = delegates = new Dictionary<string, Delegate>();
    //        delegates[propertyName] = (Delegate)(object)d;
    //        return d;
    //    }

    //    /// <summary>
    //    /// Returns true if there are any delegates associated with the given object reference.
    //    /// </summary>
    //x    internal bool _HasAccessors(Int32 id)
    //    {
    //        Dictionary<string, Delegate> delegates;
    //        return _Accessors.TryGetValue(id, out delegates) && delegates.Count > 0;
    //    }

    //    /// <summary>
    //    /// Clears any accessor delegates associated with the given object reference.
    //    /// </summary>
    //x    internal void _ClearAccessors(Int32 id)
    //    {
    //        Dictionary<string, Delegate> delegates;
    //        if (_Accessors.TryGetValue(id, out delegates))
    //            delegates.Clear();
    //    }
    //}

    // ========================================================================================================================

    /// <summary>
    ///     Represents a V8 context in which JavaScript is executed. You can call
    ///     <see cref="V8Engine.CreateContext(ObjectTemplate)"/> to create new executing contexts with a new default/custom
    ///     global object.
    /// </summary>
    /// <seealso cref="T:System.IDisposable"/>
    public unsafe class Context : IDisposable
    {
        internal NativeContext* _NativeContext;
        internal Context(NativeContext* nativeContext) { _NativeContext = nativeContext; }
        //~Context() { V8NetProxy.DeleteContext(_NativeContext); _NativeContext = null; }

        internal readonly Dictionary<Type, TypeBinder> _Binders = new();
        internal readonly Dictionary<Type, Func<TypeBinder>> _BindersLazy = new();

        //internal readonly IndexedObjectList<RootableReference> _Objects = new IndexedObjectList<RootableReference>();
        //internal readonly ReaderWriterLock _ObjectsLocker = new ReaderWriterLock();

        public int MaxDuration = 0;

        public Dictionary<string, MemorySnapshot> _memorySnapshots;
        public MemorySnapshot LastMemorySnapshotBefore = null;

        public Dictionary<string, MemorySnapshot> MemorySnapshots
        {

            get
            {
                if (_memorySnapshots == null)
                    _memorySnapshots = new Dictionary<string, MemorySnapshot>();
                return _memorySnapshots;
            }
            set
            {
                _memorySnapshots = value;
            }
        }

        public void Dispose()
        {
            _Binders.Clear();
            _BindersLazy.Clear();
            _memorySnapshots.Clear();
            LastMemorySnapshotBefore = null;

            if (_NativeContext != null)
                V8NetProxy.DeleteContext(_NativeContext);
            _NativeContext = null;
        }

        public static implicit operator Context(NativeContext* ctx) => new Context(ctx);
        public static implicit operator NativeContext* (Context ctx) => ctx._NativeContext;
    }


    public unsafe class MemorySnapshot {
        public List<Int32> ExistingHandleIDs;
        public List<Int32> ExistingObjectIDs;

        public Dictionary<Int32, int> ChildHandleIDs;

        public MemorySnapshot(V8Engine engine)
        {
            Init(engine);
        }

        public void Reset()
        {
            if (ExistingHandleIDs == null)
                ExistingHandleIDs = new List<Int32>();
            else
                ExistingHandleIDs.Clear();

            if (ExistingObjectIDs == null)
                ExistingObjectIDs = new List<Int32>();
            else
                ExistingObjectIDs.Clear();

            if (ChildHandleIDs == null)
                ChildHandleIDs = new Dictionary<Int32, int>();
            else
                ChildHandleIDs.Clear();
        }

        public void Init(V8Engine engine) 
        {
            Reset();

            for (var i = 0; i < engine._HandleProxies.Length; i++)
            {
                var hProxy = engine._HandleProxies[i];
                if (hProxy != null && !hProxy->IsCLRDisposed)
                {
                    ExistingHandleIDs.Add(i);
                }
            }

            for (var i = 0; i < engine._Objects.Count; i++)
            {
                var rootableRef = engine._Objects[i]; 
                if (rootableRef != null) {
                    InternalHandle h = ((V8NativeObject)rootableRef.Target)?._ ?? InternalHandle.Empty;
                    if (!h.IsEmpty) {
                        ExistingObjectIDs.Add(i);
                    }
                }
            }
        }

        public void Add(InternalHandle h)
        {
            ExistingHandleIDs.Add(h.HandleID);
            ExistingObjectIDs.Add(h.ObjectID);
        }
    }
}