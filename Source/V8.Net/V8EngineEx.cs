#nullable enable
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks; 

namespace V8.Net
{

    public interface IJsConverter
    {
        InternalHandle ConvertToJs(V8Engine engine, object obj, bool keepAlive = false);

        bool IsMemoryChecksOn { get; }
    }

    public class DummyJsConverter : IJsConverter
    {
        public InternalHandle ConvertToJs(V8Engine engine, object obj, bool keepAlive = false)
        {
            return InternalHandle.Empty;
        }

        public bool IsMemoryChecksOn => false;
    }

    // ========================================================================================================================
    // (.NET and Mono Marshalling: http://www.mono-project.com/Interop_with_Native_Libraries)
    // (Mono portable code: http://www.mono-project.com/Guidelines:Application_Portability)
    // (Cross platform p/invoke: http://www.gordonml.co.uk/software-development/mono-net-cross-platform-dynamic-pinvoke/)

    /// <summary>
    /// Creates a new managed V8Engine wrapper instance and associates it with a new native V8 engine.
    /// The engine does not implement locks, so to make it thread safe, you should lock against an engine instance (i.e. lock(myEngine){...}).  The native V8
    /// environment, however, is thread safe (but blocks to allow only one thread at a time).
    /// </summary>
    public unsafe partial class V8Engine
    {
        private static IJsConverter _dummyJsConverter = new DummyJsConverter();


        private IJsConverter _jsConverter;

        public bool IsMemoryChecksOn => _jsConverter?.IsMemoryChecksOn ?? false;

        protected Dictionary<Type, Func<object, InternalHandle>> TypeMappers;


        internal InternalHandle CreateUIntValue(uint v)
        {
            return v < int.MaxValue ? CreateValue((Int32) v) : CreateValue((double) v);
        }

        internal InternalHandle CreateUIntValue(ulong v)
        {
            return v < int.MaxValue ? CreateValue((Int32) v) : CreateValue((double) v);
        }

        internal InternalHandle CreateIntValue(long v)
        {
            return v < int.MaxValue && v > int.MinValue ? CreateValue((Int32) v) : CreateValue((double) v);
        }

        public InternalHandle CreateObjectBinder(object obj, TypeBinder tb = null, bool keepAlive = false)
        {
            return CreateObjectBinder<ObjectBinder>(obj, tb, keepAlive: keepAlive);
        }

        public InternalHandle CreateObjectBinder<TObjectBinder>(object obj, TypeBinder tb = null, bool keepAlive = false)
        where TObjectBinder : ObjectBinder, new()
        {
            if (obj == null)
            {
                return null;
            }
            if (tb == null)
            {
                Type type = obj is Task ? typeof(Task) : obj.GetType();
                tb = GetTypeBinder(type);
                if (tb == null)
                    throw new InvalidOperationException($"No TypeBinder found for type {nameof(type)}");
            }

            ObjectBinder binder = tb.CreateObjectBinder<TObjectBinder, object>(obj, true, keepAlive: keepAlive);

            if (IsMemoryChecksOn)
            {
                if (obj is IV8DebugInfo di)
                    di.SelfID = new V8EntityID(binder._.ID, binder._.ObjectID);
            }

            return binder._; //new InternalHandle(ref binder._, true);
        }


        public InternalHandle FromObject(object obj, bool keepAlive = false, bool createBinder = true)
        {
            if (obj == null)
                return InternalHandle.Empty;

            if (obj is InternalHandle jsValue)
                return jsValue;

            var valueType = obj.GetType();
            if (valueType.IsEnum)
            {
                return CreateValue(obj.ToString());
                
                // is overloaded with upper code
                /*Type ut = Enum.GetUnderlyingType(valueType);

                if (ut == typeof(ulong))
                    return CreateValue(System.Convert.ToDouble(obj));

                if (ut == typeof(uint) || ut == typeof(long))
                    return CreateValue(System.Convert.ToInt64(obj));

                return CreateValue(System.Convert.ToInt32(obj));*/
            }

            jsValue = _jsConverter.ConvertToJs(this, obj, keepAlive);
            if (!jsValue.IsEmpty)
                return jsValue;

            var typeMappers = TypeMappers;
            if (typeMappers.TryGetValue(valueType, out var typeMapper))
            {
                return typeMapper(obj);
            }

            /*var type = obj as Type;
            if (type != null)
            {
                var typeReference = TypeReference.CreateTypeReference(this, type);
                return typeReference;
            }*/

            if (obj is System.Array a)
            {
                // racy, we don't care, worst case we'll catch up later
                Interlocked.CompareExchange(ref TypeMappers, new Dictionary<Type, Func<object, InternalHandle>>(typeMappers)
                {
                    [valueType] = Convert
                }, typeMappers);

                return Convert(a);
            }

            if (obj is JSFunction f)
                return this.CreateFunctionTemplate().GetFunctionObject<V8Function>(f)._;

            // if no known type could be guessed, wrap it as an ObjectBinder
            return createBinder ? CreateObjectBinder(obj, keepAlive: keepAlive) : InternalHandle.Empty;
        }

        private InternalHandle Convert(object v)
        {
            return CreateArray((System.Array)v);
        }

        public InternalHandle CreateArray(System.Array items)
        {
            int arrayLength = items.Length;
            var jsItems = new InternalHandle[arrayLength];
            for (int i = 0; i < arrayLength; ++i)
            {
                jsItems[i] = FromObject(items.GetValue(i));
            }

            return CreateArrayWithDisposal(jsItems);
        }

        public InternalHandle CreateArray(IEnumerable<object> items)
        {
            var list = CreateArray(Array.Empty<InternalHandle>());
            void pushKey(object value)
            {
                using (var jsValue = FromObject(value))
                using (var jsResPush = list.StaticCall("push", jsValue))
                    jsResPush.ThrowOnError();
            }

            foreach (var item in items)
                pushKey(item);
            return list;
        }

        public static double ToNumber(InternalHandle o)
        {
            return o.AsDouble;
        }

        public void SetMaxDuration(int maxDurationNew)
        {
            _Context.MaxDuration = maxDurationNew;
        }

        public IDisposable ChangeMaxDuration(int maxDurationNew)
        {
            var context = _Context;
            int maxDurationPrev = context.MaxDuration;

            void RestoreMaxDuration()
            {
                context.MaxDuration = maxDurationPrev;
            }

            context.MaxDuration = maxDurationNew;
            return new DisposableAction(RestoreMaxDuration);
        }

        public IDisposable DisableMaxDuration()
        {
            return ChangeMaxDuration(0);
        }

        public void ResetCallStack()
        {
            //engine?.ForceV8GarbageCollection();

            // there is no need to do something as V8 doesn't have intermediate state of callstack
        }

        public void ResetConstraints()
        {
            // there is no need to do something as V8 doesn't have intermediate state of timer
        }

        public int RefineMaxDuration(int timeout)
        {
            return timeout > 0 ? timeout : _Context.MaxDuration;
        }
        
        public void ExecuteWithReset(string source, string sourceName = "anonymousCode.js", bool throwExceptionOnError = true, int timeout = 0)
        {
            using (ExecuteExprWithReset(source, sourceName, throwExceptionOnError, timeout))
            {}
        }

        public void ExecuteWithReset(InternalHandle script, bool throwExceptionOnError = true, int timeout = 0)
        {
            using (ExecuteExprWithReset(script, throwExceptionOnError, timeout))
            {}
        }

        public InternalHandle ExecuteExprWithReset(string source, string sourceName = "anonymousCode.js", bool throwExceptionOnError = true, int timeout = 0)
        {
            using (var script = Compile(source, sourceName, throwExceptionOnError))
            {
                return ExecuteExprWithReset(script, throwExceptionOnError, timeout);
            }
        }

        public InternalHandle ExecuteExprWithReset(InternalHandle script, bool throwExceptionOnError = true, int timeout = 0)
        {
            try
            {
                return Execute(script, throwExceptionOnError, RefineMaxDuration(timeout));
            }
            finally
            {
                ResetCallStack();
                ResetConstraints();
            }
        }
    }

    /// <summary>
    /// A helper class that translate between Disposable and Action
    /// </summary>
    internal class DisposableAction : IDisposable
    {
        private readonly Action _action;

        /// <summary>
        /// Initializes a new instance of the <see cref="DisposableAction"/> class.
        /// </summary>
        /// <param name="action">The action.</param>
        public DisposableAction(Action action)
        {
            _action = action;
        }

        /// <summary>
        /// Execute the relevant actions
        /// </summary>
        public void Dispose()
        {
            _action();
        }
    }
    // ========================================================================================================================
}
