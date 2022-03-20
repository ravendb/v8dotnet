// https://thlorenz.github.io/v8-dox/build/v8-3.25.30/html/index.html (more up to date)

#include "ProxyTypes.h"
#include <experimental/filesystem>
//#include "V8/v8/src/heap/heap-inl.h"
//#include "src/common/globals.h"
//#include "timezone.h"
#include "unicode/timezone.h"
#include "node_utils.h"
#include <time.h>  // tzset(), _tzset()

#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#define CHAR_TO_UCHAR(c) c
#define UCHAR_TO_CHAR(c) c
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#define CHAR_TO_UCHAR(u) asciiFromEbcdic[u]
#define UCHAR_TO_CHAR(u) ebcdicFromAscii[u]
#else
#   error U_CHARSET_FAMILY is not valid
#endif

U_CAPI void U_EXPORT2
u_charsToUChars(const char *cs, UChar *us, int32_t length) {
    UChar u;
    uint8_t c;

    /*
     * Allow the entire ASCII repertoire to be mapped _to_ Unicode.
     * For EBCDIC systems, this works for characters with codes from
     * codepages 37 and 1047 or compatible.
     */
    while(length>0) {
        c=(uint8_t)(*cs++);
        u=(UChar)CHAR_TO_UCHAR(c);
        //U_ASSERT((u!=0 || c==0)); /* only invariant chars converted? */
        *us++=u;
        --length;
    }
}

void SetDefaultTimeZone(const char* tzid) 
{
  size_t tzidlen = strlen(tzid) + 1;
  UErrorCode status = U_ZERO_ERROR;
  MaybeStackBuffer<UChar, 256> id(tzidlen);
  u_charsToUChars(tzid, id.out(), tzidlen);
  // This is threadsafe:
  ucal_setDefaultTimeZone(id.out(), &status);
  //CHECK(U_SUCCESS(status));
}

void DateTimeConfigurationChangeNotification(
    Isolate* isolate,
    const char* val = nullptr) {
#ifdef V8_OS_POSIX
    tzset();
    isolate->DateTimeConfigurationChangeNotification(
        Isolate::TimeZoneDetection::kRedetect);
#else
    _tzset();

# if defined(NODE_HAVE_I18N_SUPPORT)
    isolate->DateTimeConfigurationChangeNotification(
        Isolate::TimeZoneDetection::kSkip);

    // On windows, the TZ environment is not supported out of the box.
    // By default, v8 will only be able to detect the system configured
    // timezone. This supports using the TZ environment variable to set
    // the default timezone instead.
    SetDefaultTimeZone(val);
# else
    isolate->DateTimeConfigurationChangeNotification(
        Isolate::TimeZoneDetection::kRedetect);
# endif
#endif
}


constexpr int KB = 1024;
constexpr int MB = KB * 1024;
constexpr int GB = MB * 1024;
constexpr int64_t TB = static_cast<int64_t>(GB) * 1024;
static constexpr size_t kPageSize = size_t{1} << 17;

// ------------------------------------------------------------------------------------------------------------------------

_StringItem::_StringItem() : String(nullptr), Length(0) { }
_StringItem::_StringItem(V8EngineProxy *engine, size_t length)
{
	Engine = engine;
	Length = length;
	String = (uint16_t*)ALLOC_MANAGED_MEM(sizeof(uint16_t) * (length + 1));
}
_StringItem::_StringItem(V8EngineProxy *engine, v8::String* str)
{
	Engine = engine;
	Length = str->Length();
	String = (uint16_t*)ALLOC_MANAGED_MEM(sizeof(uint16_t) * (Length + 1));
	str->Write(Engine->Isolate(), String);
}

void _StringItem::Free() { if (String != nullptr) { FREE_MANAGED_MEM(String); String = nullptr; } }

_StringItem _StringItem::ResizeIfNeeded(size_t newLength)
{
	if (newLength > Length)
	{
		Length = newLength;
		if (String == nullptr)
			String = (uint16_t*)ALLOC_MANAGED_MEM(sizeof(uint16_t) * (Length + 1));
		else
			String = (uint16_t*)REALLOC_MANAGED_MEM(String, sizeof(uint16_t) * (Length + 1));
	}
	return *this;
}
void _StringItem::Dispose() { if (Engine != nullptr) Engine->DisposeNativeString(*this); }
void _StringItem::Clear() { String = nullptr; Length = 0; }

// ------------------------------------------------------------------------------------------------------------------------

static bool _V8Initialized = false;

std::vector<bool> V8EngineProxy::_DisposedEngines(100, false);

int32_t V8EngineProxy::_NextEngineID = 0;

// ------------------------------------------------------------------------------------------------------------------------

bool V8EngineProxy::IsDisposed(int32_t engineID)
{
	return engineID < 0 || _DisposedEngines[engineID];
}

bool V8EngineProxy::IsExecutingScript()
{
	return _IsExecutingScript || _InCallbackScope > 0;
}

// ------------------------------------------------------------------------------------------------------------------------

Isolate* V8EngineProxy::Isolate() { return _Isolate; }

Handle<v8::Context> V8EngineProxy::Context() { return _Context; }

// ------------------------------------------------------------------------------------------------------------------------

//class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
//public:
//	virtual void* Allocate(size_t length) { return malloc(length); }
//	virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
//	virtual void Free(void* data, size_t length) { free(data); }
//};

V8EngineProxy::V8EngineProxy(bool enableDebugging, DebugMessageDispatcher* debugMessageDispatcher, int debugPort)
	:ProxyBase(V8EngineProxyClass), /*?_GlobalObjectTemplateProxy(nullptr),*/ _NextNonTemplateObjectID(-2),
	_IsExecutingScript(false), _InCallbackScope(0), _IsTerminatingScript(false), _Handles(1000, nullptr), _HandlesPendingDisposal(1000, nullptr), _DisposedHandles(1000, -1), _HandlesToBeMadeWeak(1000, nullptr),
	_HandlesToBeMadeStrong(1000, nullptr), _Objects(1000, nullptr), _Strings(1000, _StringItem())
{
	if (!_V8Initialized) // (the API changed: https://groups.google.com/forum/#!topic/v8-users/wjMwflJkfso)
	{
		v8::V8::InitializeICU();

		//?v8::V8::InitializeExternalStartupData(PLATFORM_TARGET "\\");
		// (Startup data is not included by default anymore)

		_Platform = v8::platform::NewDefaultPlatform();
		v8::V8::InitializePlatform(_Platform.get());

		v8::V8::Initialize();

		_V8Initialized = true;
	}

	Isolate::CreateParams params;
	ResourceConstraints& constraints = params.constraints;
	//constraints.ConfigureDefaults();
	constraints.ConfigureDefaultsFromHeapSize(10 * KB, 10 * GB);
	constraints.set_code_range_size_in_bytes(9 * GB);
	constraints.set_initial_young_generation_size_in_bytes(MB);
	//constraints.set_initial_old_generation_size_in_bytes(MB);

	params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	_Isolate = Isolate::New(params);

	BEGIN_ISOLATE_SCOPE(this);

	_Handles.clear();
	_HandlesPendingDisposal.clear();
	_DisposedHandles.clear();
	_HandlesToBeMadeWeak.clear();
	_HandlesToBeMadeStrong.clear();
	_Objects.clear();
	_Strings.clear();

	_ManagedV8GarbageCollectionRequestCallback = nullptr;

	_Isolate->SetData(0, this); // (sets a reference in the isolate to the proxy [useful within callbacks])
	
	//Heap* heap = _Isolate->heap();
	auto nearHeapLimitCallback = [](void* heap, size_t current_heap_limit, size_t initial_heap_limit) -> size_t
	{
		//reinterpret_cast<Heap*>(heap)->set_force_oom(false);
		auto step = current_heap_limit * 2;
		if (step > 10 * MB) {
			step = 10 * MB;
		}
		auto limit = current_heap_limit + step;
		printf("\nnearHeapLimitCallback\n%s\n", limit);
		return limit;
	};
	_Isolate->AddNearHeapLimitCallback(nearHeapLimitCallback, nullptr);

	if ((std::vector<bool>::size_type)_NextEngineID >= _DisposedEngines.capacity())
		_DisposedEngines.resize(_DisposedEngines.capacity() + 32);

	if (_NextEngineID == 0)
		_DisposedEngines.clear(); // (need to clear the pre-allocated vector on first use)
	_DisposedEngines.push_back(false);
	_EngineID = _NextEngineID++;

	setenv("TZ", "UTC", 1);
	DateTimeConfigurationChangeNotification(_Isolate, "UTC");

	END_ISOLATE_SCOPE;
}

// ------------------------------------------------------------------------------------------------------------------------

V8EngineProxy::~V8EngineProxy()
{
	if (Type != 0) // (type is 0 if this class was wiped with 0's {if used in a marshalling test})
	{
		lock_guard<recursive_mutex> handleSection(_HandleSystemMutex);

		BEGIN_ISOLATE_SCOPE(this);

		// ... empty all handles to be sure they won't be accessed ...

		for (size_t i = 0; i < _Handles.size(); i++)
			_Handles[i]->_ClearHandleValue();

		// ... flag engine as disposed ...

		_DisposedEngines[_EngineID] = true; // (this supports cases where the engine may be deleted while proxy objects are still in memory)
		// (note: once this flag is set, disposing handles causes the proxy instances to be deleted immediately [instead of caching])

		// ... deleted disposed proxy handles ...

		// At this point the *disposed* (and hence, *cached*) proxy handles are no longer associated with managed handles, so the engine is now responsible to delete them)
		for (size_t i = 0; i < _DisposedHandles.size(); i++)
			_Handles[_DisposedHandles[i]]->_Dispose(false); // (engine is flagged as disposed, so this call will only delete the instance)

		// Note: the '_GlobalObjectTemplateProxy' instance is not deleted because the managed GC will do that later (if not before this).
		//?_GlobalObjectTemplateProxy = nullptr;

		if (!_GlobalObject.IsEmpty())
			_GlobalObject.Reset();

		if (!_Context.IsEmpty())
			_Context.Reset();

		END_ISOLATE_SCOPE;

		_Isolate->Dispose();
		_Isolate = nullptr;

		_Platform.release();

		// ... free the string cache ...

		for (size_t i = 0; i < _Strings.size(); i++)
			_Strings[i].Free();
	}
}

// ------------------------------------------------------------------------------------------------------------------------

/**
* Converts a given V8 string into a uint16_t* string using ALLOC_MANAGED_MEM().
* The string is expected to be freed by calling FREE_MANAGED_MEM(), or within a managed assembly.
*/
_StringItem V8EngineProxy::GetNativeString(v8::String* str)
{
	_StringItem _str;

	auto size = _Strings.size();

	if (size > 0)
	{
		_str = _Strings[size - 1].ResizeIfNeeded(str->Length());
		_Strings[size - 1].Clear();
		_Strings.pop_back();
	}
	else
	{
		_str = _StringItem(this, str == nullptr ? 0 : str->Length());
	}

	if (str != nullptr)
		str->Write(_Isolate, _str.String);
	else
		_str.String = nullptr;

	return _str;
}

/**
* Puts the string back into the cache for reuse.
*/
void V8EngineProxy::DisposeNativeString(_StringItem &item)
{
	_Strings.push_back(item);
}

// ------------------------------------------------------------------------------------------------------------------------
/*
 * Returns a proxy wrapper for the given handle to allow access via the managed side.
 */
HandleProxy* V8EngineProxy::GetHandleProxy(Handle<Value> handle)
{
	HandleProxy* handleProxy = nullptr;

	// ... first check if this handle is an object with an ID, and if so, try to pull an existing handle ...

	auto id = HandleProxy::GetManagedObjectID(handle);

	if (id >= 0 && id < (int)_Objects.size())
		handleProxy = _Objects.at(id);

	if (handleProxy == nullptr)
	{
		lock_guard<recursive_mutex> handleSection(_HandleSystemMutex);

		ProcessHandleQueues(2);

		if (_DisposedHandles.size() == 0)
		{
			// (no handles are disposed/cached, which means a new one is required)
			// ... try to trigger disposal of weak handles ...
			//if (_HandlesToBeMadeWeak.size() > 1000)
			_Isolate->IdleNotificationDeadline(100); // (handles should not have to be created all the time, so this helps to free them up if too many start adding up in weak state)
		}

		if (_DisposedHandles.size() > 0)
		{
			auto id = _DisposedHandles.back();
			_DisposedHandles.pop_back();
			handleProxy = _Handles.at(id);
#if DEBUG
			if (handleProxy->_EngineID < -2 || handleProxy->_EngineID > 1000)
				throw runtime_error("V8EngineProxy::GetHandleProxy(): Assertion failed: The engine ID for the disposed proxy handle does not look right.");
#endif
			handleProxy->_EngineProxy = this;
			handleProxy->_EngineID = _EngineID;
			handleProxy = handleProxy->Initialize(handle); // (can return null if the engine is gone)
		}
		else
		{
			handleProxy = (new HandleProxy(this, (int32_t)_Handles.size()))->Initialize(handle);

			if (handleProxy != nullptr)
			{
				_Handles.push_back(handleProxy); // (keep a record of all handles created)

				ProcessHandleQueues(10); // (process one more time to make this twice as fast as long as new handles are being created)
			}
		}
	}

	if (handleProxy == nullptr) throw runtime_error("V8EngineProxy::GetHandleProxy(): The engine is gone! Cannot create any handles.");

	return handleProxy;
}

void V8EngineProxy::QueueHandleDisposal(HandleProxy *handleProxy)
{
	if (handleProxy != nullptr && !handleProxy->IsDisposed() && !handleProxy->IsDisposing())
	{
		handleProxy->_Disposed |= 16;
		lock_guard<std::mutex> handleSection(_DisposingHandleMutex); // NO V8 HANDLE ACCESS HERE BECAUSE OF THE MANAGED GC
		bool isAlreadyIn = false;
		if (_HandlesPendingDisposal.size() > 0)
		{
			auto hLast = _HandlesPendingDisposal.back();
			isAlreadyIn = hLast->_ID == handleProxy->_ID;
		}

		/*for(auto h : _HandlesPendingDisposal) {
			if (h->_ID == handleProxy->_ID) {
				isAlreadyIn = true;
				break;
			}
		}*/
		if (!isAlreadyIn)
			_HandlesPendingDisposal.push_back(handleProxy);
	}
}

void V8EngineProxy::DisposeHandleProxy(HandleProxy *handleProxy)
{
	if (handleProxy == nullptr || handleProxy->IsDisposed()) return;

	// .. the GC finalizer might end up here, so try to get a lock first and queue the request on failure instead ...

	std::unique_lock<recursive_mutex> lock(_HandleSystemMutex, std::try_to_lock);

	if (!lock.owns_lock()) {
		QueueHandleDisposal(handleProxy);
		return;
	}

	if (handleProxy->_ObjectID >= 0 && handleProxy->_ObjectID < (int)_Objects.size())
		_Objects[handleProxy->_ObjectID] = nullptr;

	if (handleProxy->_Dispose(false))
	{
#if DEBUG
		if (!_DisposedHandles.empty() && std::find(_DisposedHandles.begin(), _DisposedHandles.end(), handleProxy->_ID) != _DisposedHandles.end())
			throw runtime_error("DisposeHandleProxy(): A handle ID already exists! There should not be two of the same IDs in the queue.");
#endif
		_DisposedHandles.push_back(handleProxy->_ID); // (this is a queue of disposed handles to use for recycling; Note: the persistent handles are NEVER disposed until they become reinitialized)
	}
}

// ------------------------------------------------------------------------------------------------------------------------

void V8EngineProxy::QueueMakeWeak(HandleProxy *handleProxy)
{
	lock_guard<recursive_mutex> makeWeakSection(_MakeWeakQueueMutex); // NO V8 HANDLE ACCESS HERE BECAUSE OF THE MANAGED GC

	if ((handleProxy->_Disposed & 4) == 0)
	{
		handleProxy->_Disposed |= 4;
		_HandlesToBeMadeWeak.push_back(handleProxy);
	}
}

void V8EngineProxy::QueueMakeStrong(HandleProxy *handleProxy) // TODO: "MakeStrong" requests may no longer be needed.
{
	lock_guard<recursive_mutex> makeStrongSection(_MakeStrongQueueMutex); // NO V8 HANDLE ACCESS HERE BECAUSE OF THE MANAGED GC

	if ((handleProxy->_Disposed & 8) == 0)
	{
		handleProxy->_Disposed |= 8;
		_HandlesToBeMadeStrong.push_back(handleProxy);
	}
}

void V8EngineProxy::ProcessHandleQueues(int loops)
{
	bool didSomething = true;

	while (loops-- > 0 && didSomething)
	{
		// ... process one of each per call ...

		HandleProxy * h;
		didSomething = false;

		if (_HandlesPendingDisposal.size() > 0 && _InCallbackScope == 0)
		{
			lock_guard<std::mutex> handleSection(_DisposingHandleMutex); // NO V8 HANDLE ACCESS HERE BECAUSE OF THE MANAGED GC
			if (_HandlesPendingDisposal.size() > 0)
			{
				h = _HandlesPendingDisposal.back();
				_HandlesPendingDisposal.pop_back();
				h->_Disposed &= ~(int32_t)16; // (clear this flag; no longer in the queue)
				h->Dispose(); // (only returns  false is the engine is no longer available)
			}
		}

		if (_HandlesToBeMadeWeak.size() > 0)
		{
			lock_guard<recursive_mutex> makeWeakSection(_MakeWeakQueueMutex); // PROTECTS AGAINST THE WORKER THREAD - but only when the queue is not empty to be more efficient.
			if (_HandlesToBeMadeWeak.size() > 0)
			{
				h = _HandlesToBeMadeWeak.back();
				_HandlesToBeMadeWeak.pop_back();
				h->_Disposed &= ~(int32_t)4; // (remove queued flag is exists)
				h->_Disposed &= ~16; // (clear this flag; no longer in the queue)
				h->MakeWeak();
				didSomething = true;
			}
		}

		if (_HandlesToBeMadeStrong.size() > 0)
		{
			lock_guard<recursive_mutex> makeStrongSection(_MakeStrongQueueMutex); // PROTECTS AGAINST THE WORKER THREAD - but only when the queue is not empty to be more efficient.
			if (_HandlesToBeMadeStrong.size() > 0)
			{
				h = _HandlesToBeMadeStrong.back();
				_HandlesToBeMadeStrong.pop_back();
				h->_Disposed &= ~(int32_t)8; // (clear this flag; no longer in the queue)
				h->MakeStrong(); // TODO: "MakeStrong" requests may no longer be needed.
				didSomething = true;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------

void  V8EngineProxy::RegisterGCCallback(ManagedV8GarbageCollectionRequestCallback managedV8GarbageCollectionRequestCallback)
{
	_ManagedV8GarbageCollectionRequestCallback = managedV8GarbageCollectionRequestCallback;
}

// ------------------------------------------------------------------------------------------------------------------------

ObjectTemplateProxy* V8EngineProxy::CreateObjectTemplate()
{
	return new ObjectTemplateProxy(this);
}

FunctionTemplateProxy* V8EngineProxy::CreateFunctionTemplate(uint16_t *className, ManagedJSFunctionCallback callback)
{
	return new FunctionTemplateProxy(this, className, callback);
}

// ------------------------------------------------------------------------------------------------------------------------

// Creates a new context and returns it.
ContextProxy* V8EngineProxy::CreateContext(ObjectTemplateProxy* templateProxy)
{
	if (templateProxy == nullptr)
		templateProxy = CreateObjectTemplate();

	auto context = v8::Context::New(_Isolate, nullptr, Local<ObjectTemplate>::New(_Isolate, templateProxy->_ObjectTemplate));

	// ... the context auto creates the global object from the given template, BUT, we still need to update the internal fields with proper values expected
	// for callback into managed code ...

	context->Enter(); // (in case we need this now)

	auto globalObject = ToLocalThrow(context->Global()->GetPrototype()->ToObject(_Context), "Failed to get global object");
	globalObject->SetAlignedPointerInInternalField(0, templateProxy); // (proxy object reference)
	globalObject->SetInternalField(1, External::New(_Isolate, (void*)-1)); // (manage object ID, which is only applicable when tracking many created objects [and not a single engine or global scope])

	CopyablePersistent<v8::Context> contextCP(context);
	auto contextProxy = new ContextProxy(this, contextCP); // (the native side will own this, and is responsible to free it when done)

	//_Inspector = std::unique_ptr<Inspector>(new Inspector(_Platform, context, _InspectorPort + templateProxy->_EngineID));
	//_Inspector->startAgent();

	context->Exit();

	return contextProxy;
}

// Sets the context and returns a handle to the new global object.
HandleProxy* V8EngineProxy::SetContext(ContextProxy* contextProxy)
{
	//?if (_GlobalObjectTemplateProxy != nullptr)
	//?	delete _GlobalObjectTemplateProxy;

	//?_GlobalObjectTemplateProxy = proxy;

	if (!_Context.IsEmpty())
		_Context.Reset(); // (release the handle first)

	if (!_GlobalObject.IsEmpty())
		_GlobalObject.Reset(); // (release the handle first)

	_Context = contextProxy->_Context; // (set the new context)
	auto global = ToLocalThrow(_Context->Global()->GetPrototype()->ToObject(_Context), "Failed to get global object"); // (keep a reference to the global object for faster reference)
	_GlobalObject = global; // (set the new global object)

	return GetHandleProxy(global);
}

// Sets the context and returns a handle to the new global object.
ContextProxy* V8EngineProxy::GetContext()
{
	return new ContextProxy(this, _Context); // (the native side will own this, and is responsible to free it when done)
}

// ------------------------------------------------------------------------------------------------------------------------

Local<String> V8EngineProxy::GetErrorMessage(Local<v8::Context> ctx, TryCatch &tryCatch)
{
	auto isolate = ctx->GetIsolate();
	auto msg = tryCatch.Message();
	auto messageExists = !msg.IsEmpty();
	auto excep = tryCatch.Exception();
	auto exceptionExists = !excep.IsEmpty();
	Local<Value> stack;
	bool stackExists = tryCatch.StackTrace(ctx).ToLocal(&stack) && !stack->IsUndefined();

	Local<String> stackStr;

	if (stackExists && exceptionExists)
	{
		stackStr = stack->ToString(ctx).FromMaybe(Local<String>());

		auto exceptionMsg = tryCatch.Exception()->ToString(ctx).FromMaybe(Local<String>());

		// ... detect if the start of the stack message is the same as the exception message, then remove it (seems to happen when managed side returns an error) ...

		if (stackStr->Length() >= exceptionMsg->Length())
		{
			uint16_t* ss = new uint16_t[stackStr->Length() + 1];
			stack->ToString(ctx).FromMaybe(Local<String>())->Write(isolate, ss); // (copied to a new array in order to offset the character pointer to extract a substring)

			// ... get the same number of characters from the stack message as the exception message length ...
			auto subStackStr = NewSizedUString(ss, exceptionMsg->Length()).FromMaybe(Local<String>());

			if (exceptionMsg->Equals(ctx, subStackStr).FromMaybe(false))
			{
				// ... using the known exception message length, ...
				auto stackPartStr = NewSizedUString(ss + exceptionMsg->Length(), stackStr->Length() - exceptionMsg->Length()).FromMaybe(Local<String>());
				stackStr = stackPartStr;
			}

			delete[] ss;
		}
	}

	auto msgStr = (messageExists ? msg->Get() : NewString("")).FromMaybe(Local<String>());

	if (tryCatch.HasTerminated())
	{
		if (msgStr->Length() > 0)
			msgStr = msgStr->Concat(isolate, msgStr, NewString("\r\n").FromMaybe(Local<String>()));
		msgStr = msgStr->Concat(isolate, msgStr, NewString("Script execution aborted by request.").FromMaybe(Local<String>()));
	}

	if (messageExists)
	{
		msgStr = msgStr->Concat(isolate, msgStr, NewString("\r\n").FromMaybe(Local<String>()));

		msgStr = msgStr->Concat(isolate, msgStr, NewString("  Line: ").FromMaybe(Local<String>()));
		auto line = NewInteger(ToThrow(msg->GetLineNumber(ctx)))->ToString(ctx).FromMaybe(Local<String>());
		msgStr = msgStr->Concat(isolate, msgStr, line);

		msgStr = msgStr->Concat(isolate, msgStr, NewString("  Column: ").FromMaybe(Local<String>()));
		auto col = NewInteger(ToThrow(msg->GetStartColumn(ctx)))->ToString(ctx).FromMaybe(Local<String>());
		msgStr = msgStr->Concat(isolate, msgStr, col);
	}

	if (stackExists)
	{
		msgStr = msgStr->Concat(isolate, msgStr, NewString("\r\n").FromMaybe(Local<String>()));

		msgStr = msgStr->Concat(isolate, msgStr, NewString("  Stack: ").FromMaybe(Local<String>()));
		msgStr = msgStr->Concat(isolate, msgStr, stackStr);
	}

	msgStr = msgStr->Concat(isolate, msgStr, NewString("\r\n").FromMaybe(Local<String>()));

	return msgStr;
}

HandleProxy* V8EngineProxy::Execute(const uint16_t* script, uint16_t* sourceName)
{
	HandleProxy *returnVal = nullptr;

	try
	{

		TryCatch __tryCatch(_Isolate);
		//__tryCatch.SetVerbose(true);

		if (sourceName == nullptr) sourceName = (uint16_t*)L"";

		ScriptOrigin origin(_Isolate, ToLocalThrow(NewUString(sourceName)));
		auto compiledScript = Script::Compile(_Context, ToLocalThrow(NewUString(script)), &origin);

		if (__tryCatch.HasCaught())
		{
			returnVal = GetHandleProxy(GetErrorMessage(_Context, __tryCatch));
			returnVal->_Type = JSV_CompilerError;
		}
		else if (!compiledScript.IsEmpty())
			returnVal = Execute(ToLocalThrow(compiledScript));
	}
	catch (runtime_error ex)
	{
		returnVal = GetHandleProxy(ToLocalThrow(NewString(ex.what())));
		returnVal->_Type = JSV_InternalError;
	}

	return returnVal;
}

HandleProxy* V8EngineProxy::Execute(Handle<Script> script)
{
	HandleProxy *returnVal = nullptr;

	try
	{
		TryCatch __tryCatch(_Isolate);
		//__tryCatch.SetVerbose(true);

		_IsExecutingScript = true;
		auto result = script->Run(_Context);
		_IsExecutingScript = false;

		if (__tryCatch.HasCaught())
		{
			returnVal = GetHandleProxy(GetErrorMessage(_Context, __tryCatch));
			returnVal->_Type = __tryCatch.HasTerminated() ? JSV_ExecutionTerminated : JSV_ExecutionError;
		}
		else  if (!result.IsEmpty())
			returnVal = GetHandleProxy(ToLocalThrow(result));

		_IsTerminatingScript = false;
	}
	catch (runtime_error ex)
	{
		returnVal = GetHandleProxy(NewString(ex.what()).FromMaybe(Local<String>()));
		returnVal->_Type = JSV_InternalError;
	}

	return returnVal;
}

HandleProxy* V8EngineProxy::Compile(const uint16_t* script, uint16_t* sourceName)
{
	HandleProxy *returnVal = nullptr;

	try
	{
		TryCatch __tryCatch(_Isolate);
		//__tryCatch.SetVerbose(true);

		if (sourceName == nullptr) sourceName = (uint16_t*)L"";

		auto hScript = ToLocalThrow(NewUString(script));

		ScriptOrigin origin(_Isolate, ToLocalThrow(NewUString(sourceName)));

		auto compiledScript = Script::Compile(_Context, hScript, &origin);

		if (__tryCatch.HasCaught())
		{
			returnVal = GetHandleProxy(GetErrorMessage(_Context, __tryCatch));
			returnVal->_Type = JSV_CompilerError;
		}
		else if (!compiledScript.IsEmpty())
		{
			returnVal = GetHandleProxy(Handle<Value>());
			returnVal->SetHandle(ToLocalThrow(compiledScript));
			returnVal->_Value.V8String = _StringItem(this, *hScript).String;
		}
	}
	catch (runtime_error ex)
	{
		returnVal = GetHandleProxy(NewString(ex.what()).FromMaybe(Local<String>()));
		returnVal->_Type = JSV_InternalError;
	}

	return returnVal;
}

void V8EngineProxy::TerminateExecution()
{
	if (_IsExecutingScript)
	{
		_IsExecutingScript = false;
		_IsTerminatingScript = true;
		_Isolate->TerminateExecution();
	}
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy* V8EngineProxy::Call(HandleProxy *subject, const uint16_t *functionName, HandleProxy *_this, uint16_t argCount, HandleProxy** args)
{
	if (_this == nullptr) _this = subject; // (assume the subject is also "this" if not given)

	auto hThis = _this->Handle();
	if (hThis.IsEmpty() || !hThis->IsObject())
		throw runtime_error("Call: The target instance handle ('this') does not represent an object.");

	auto hSubject = subject->Handle();
	Handle<Function> hFunc;

	if (functionName != nullptr) // (if no name is given, assume the subject IS a function object, otherwise get the property as a function)
	{
		if (hSubject.IsEmpty() || !hSubject->IsObject())
			throw runtime_error("Call: The subject handle does not represent an object.");

		auto hProp = ToLocalThrow(hSubject.As<Object>()->Get(_Context, ToLocalThrow(NewUString(functionName))));

		if (hProp.IsEmpty() || !hProp->IsFunction())
			throw runtime_error("Call: The specified property does not represent a function.");

		hFunc = hProp.As<Function>();
	}
	else if (hSubject.IsEmpty() || !hSubject->IsFunction())
		throw runtime_error("Call: The subject handle does not represent a function.");
	else
		hFunc = hSubject.As<Function>();

	TryCatch __tryCatch(_Isolate);

	MaybeLocal<Value> result;

	if (argCount > 0)
	{
		Handle<Value>* _args = new Handle<Value>[argCount];
		for (auto i = 0; i < argCount; i++)
			_args[i] = args[i]->Handle();
		result = hFunc->Call(_Context, hThis.As<Object>(), argCount, _args);
		delete[] _args;
	}
	else result = hFunc->Call(_Context, hThis.As<Object>(), 0, nullptr);

	HandleProxy *returnVal;

	if (__tryCatch.HasCaught())
	{
		returnVal = GetHandleProxy(GetErrorMessage(_Context, __tryCatch));
		returnVal->_Type = JSV_ExecutionError;
	}
	else returnVal = result.IsEmpty() ? nullptr : GetHandleProxy(ToLocalThrow(result));

	return returnVal;
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy* V8EngineProxy::CreateNumber(double num)
{
	return GetHandleProxy(NewNumber(num));
}

HandleProxy* V8EngineProxy::CreateInteger(int32_t num)
{
	return GetHandleProxy(NewInteger(num));
}

HandleProxy* V8EngineProxy::CreateBoolean(bool b)
{
	return GetHandleProxy(NewBool(b));
}

HandleProxy* V8EngineProxy::CreateString(const uint16_t* str)
{
	return GetHandleProxy(ToLocalThrow(NewUString(str)));
}

Local<Private> V8EngineProxy::CreatePrivateString(const char* value)
{
	return Private::ForApi(_Isolate, ToLocalThrow(NewString(value))); // ('ForApi' is required, otherwise a new "virtual" symbol reference of some sort will be created with the same name on each request [duplicate names, but different symbols virtually])
}

void V8EngineProxy::SetObjectPrivateValue(Local<Object> obj, const char* name, Local<Value> value)
{
	obj->SetPrivate(_Context, CreatePrivateString("ManagedObjectID"), value);
}

Local<Value> V8EngineProxy::GetObjectPrivateValue(Local<Object> obj, const char* name)
{
	auto phandle = obj->GetPrivate(_Context, CreatePrivateString("ManagedObjectID"));
	if (phandle.IsEmpty()) return V8Undefined;
	return ToLocalThrow(phandle);
}

HandleProxy* V8EngineProxy::CreateError(const uint16_t* message, JSValueType errorType)
{
	if (errorType >= 0) throw runtime_error("Invalid error type.");
	auto h = GetHandleProxy(NewUString(message).FromMaybe(Local<String>()));
	h->_Type = errorType;
	return h;
}
HandleProxy* V8EngineProxy::CreateError(const char* message, JSValueType errorType)
{
	if (errorType >= 0) throw runtime_error("Invalid error type.");
	auto h = GetHandleProxy(NewString(message).FromMaybe(Local<String>()));
	h->_Type = errorType;
	return h;
}


HandleProxy* V8EngineProxy::CreateDate(double ms)
{
	return GetHandleProxy(NewDate(_Context, ms));
}

HandleProxy* V8EngineProxy::CreateObject(int32_t managedObjectID)
{
	if (managedObjectID == -1)
		managedObjectID = GetNextNonTemplateObjectID();

	auto handle = GetHandleProxy(NewObject());
	ConnectObject(handle, managedObjectID, nullptr);
	return handle;
}

HandleProxy* V8EngineProxy::CreateArray(HandleProxy** items, uint16_t length)
{
	Local<Array> array = NewArray(length);

	if (items != nullptr && length > 0)
		for (auto i = 0; i < length; i++)
			array->Set(_Context, i, items[i]->_Handle);

	return GetHandleProxy(array);
}

HandleProxy* V8EngineProxy::CreateArray(uint16_t** items, uint16_t length)
{
	Local<Array> array = NewArray(length);

	if (items != nullptr && length > 0)
		for (auto i = 0; i < length; i++)
			array->Set(_Context, i, ToLocalThrow(NewUString(items[i])));

	return GetHandleProxy(array);
}

HandleProxy* V8EngineProxy::CreateNullValue()
{
	return GetHandleProxy(V8Null);
}

// ------------------------------------------------------------------------------------------------------------------------
