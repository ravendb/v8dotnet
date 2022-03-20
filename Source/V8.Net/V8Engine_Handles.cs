﻿using System;
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
    // The handles section has methods to deal with creating and disposing of managed handles (which wrap native V8 handles).
    // This helps to reuse existing handles to prevent having to create new ones every time, thus greatly speeding things up.

    public unsafe partial class V8Engine
    {
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Holds an index of all handles created for this engine instance.
        /// This is a managed side reference to all the active and cached native side handle wrapper (proxy) objects.
        /// </summary>
        internal HandleProxy*[] _HandleProxies = new HandleProxy*[1000];

#if TRACE
        /// <summary>
        /// Holds a list of call stacks at the time each proxy was discovered.
        /// </summary>
        internal string[] _HandleProxyDiscoveryStacks = new string[1000];
#endif

        /// <summary>
        /// When a new managed side handle wraps a native handle proxy the disposal process happens internally in a controlled 
        /// manor.  There is no need to burden the end user with tracking handles for disposal, so when a handle enters public
        /// space, it is assigned a 'HandleTracker' object reference from this list.  If not available, one is created.
        /// For instance, during a callback, all native arguments (proxy references) are converted into handle values (which in
        /// many cases means on the stack, or CPU registers, instead of the heap; though this is CLR implementation dependent).
        /// These are then passed on to the user callback method WITHOUT a handle tracker, and disposed automatically on return.
        /// This can save creating many unnecessary objects for the managed GC to deal with.
        /// </summary>
        internal CountedReference[] _TrackerHandles = new CountedReference[1000];


        public bool Reset(Int32 handleID)
        {
            if (handleID > 0) {
                var wref = GetCountedReference(handleID);
                wref?.Reset();

                if (handleID < _HandleProxies.Length) {
                    _HandleProxies[handleID] = null;
                }
                return true;
            }
            return false;
        }


        public CountedReference GetCountedReference(Int32 handleID)
        {
            CountedReference wref = null;
            if (handleID >= 0) {
                /*if (handleID >= _TrackerHandles.Length && createIfMissing) {
                    GetTrackableHandle(createIfMissing);
                }*/
                if (handleID < _TrackerHandles.Length) {
                    wref = _TrackerHandles[handleID];
                }
            }
            return wref;
        }




        /// <summary>
        /// Returns all the handles currently known on the managed side.
        /// Each InternalHandle is only a wrapper for a tracked HandleProxy native object and does not need to be disposed.
        /// Because of this, no reference counts are incremented, and thus, disposing them may destroy handles in use.
        /// This list is mainly provided for debugging purposes only.
        /// </summary>
        public IEnumerable<InternalHandle> Handles_All
        {
            get
            {
                lock (_HandleProxies)
                {
                    List<InternalHandle> handles = new List<InternalHandle>(_HandleProxies.Length);
                    for (var i = 0; i < _HandleProxies.Length; ++i)
                        if (_HandleProxies[i] != null)
                            handles.Add(InternalHandle.GetUntrackedHandleFromProxy(_HandleProxies[i]));
                    return handles.ToArray();
                }
            }
        }

        public IEnumerable<InternalHandle> Handles_Active { get { return from h in Handles_All where h._HandleProxy->IsActive select h; } }
        public IEnumerable<InternalHandle> Handles_Disposing { get { return from h in Handles_All where h.IsDisposing select h; } }
        public IEnumerable<InternalHandle> Handles_ManagedSideDisposed { get { return from h in Handles_All where !h.IsDisposed && h.IsCLRDisposed select h; } }
        public IEnumerable<InternalHandle> Handles_DisposedAndCached { get { return from h in Handles_All where h._HandleProxy->IsDisposed select h; } }

        /// <summary>
        /// Total number of handle proxy references in the V8.NET system (for proxy use).
        /// </summary>
        public int TotalHandles
        {
            get
            {
                lock (_HandleProxies)
                {
                    var c = 0;
                    foreach (var item in _HandleProxies)
                        if (item != null) c++;
                    return c;
                }
            }
        }

        /// <summary>
        /// Total number of handle proxy references that are in a native side queue for disposal.
        /// </summary>
        public int TotalHandlesPendingDisposal
        {
            get
            {
                lock (_HandleProxies)
                {
                    var c = 0;
                    foreach (var item in _HandleProxies)
                        if (item != null && item->IsDisposing) c++;
                    return c;
                }
            }
        }

        /// <summary>
        /// Total number of handles in the V8.NET system that are cached and ready to be reused.
        /// </summary>
        public int TotalHandlesCached
        {
            get
            {
                lock (_HandleProxies)
                {
                    var c = 0;
                    foreach (var item in _HandleProxies)
                        if (item != null && item->IsDisposed) c++;
                    return c;
                }
            }
        }

        /// <summary>
        /// Total number of handles in the V8.NET system that are currently in use.
        /// </summary>
        public int TotalHandlesInUse
        {
            get
            {
                lock (_HandleProxies)
                {
                    var c = 0;
                    foreach (var item in _HandleProxies)
                        if (item != null && item->IsActive) c++;
                    return c;
                }
            }
        }

        // --------------------------------------------------------------------------------------------------------------------

        void _Initialize_Handles()
        {
        }

        // --------------------------------------------------------------------------------------------------------------------
    }

    // ========================================================================================================================
}
