#include "ProxyTypes.h"

// ------------------------------------------------------------------------------------------------------------------------

FunctionTemplateProxy::FunctionTemplateProxy(V8EngineProxy* engineProxy, uint16_t* className, ManagedJSFunctionCallback managedCallback)
	:ProxyBase(FunctionTemplateProxyClass), _EngineProxy(engineProxy), _EngineID(engineProxy->_EngineID)
{
	// The function template will call the local "InvocationCallbackProxy" function, which then translates the call for the managed side.
	_FunctionTemplate = CopyablePersistent<FunctionTemplate>(NewFunctionTemplate(InvocationCallbackProxy, NewExternal(this)));

	_FunctionTemplate->SetClassName(ToLocalThrow(NewUString(className)));

	_InstanceTemplate = new ObjectTemplateProxy(_EngineProxy, _FunctionTemplate->InstanceTemplate());
	_PrototypeTemplate = new ObjectTemplateProxy(_EngineProxy, _FunctionTemplate->PrototypeTemplate());

	SetManagedCallback(managedCallback);
}

V8EngineProxy* FunctionTemplateProxy::EngineProxy() { return _EngineID >= 0 && !V8EngineProxy::IsDisposed(_EngineID) ? _EngineProxy : nullptr; }

FunctionTemplateProxy::~FunctionTemplateProxy()
{
	if (Type != 0) // (type is 0 if this class was wiped with 0's {if used in a marshalling test})
	{
		// Note: the '_InstanceTemplate' and '_PrototypeTemplate' instances are not deleted because the managed GC will do that later.
		_InstanceTemplate = nullptr;
		_PrototypeTemplate = nullptr;

		if (!V8EngineProxy::IsDisposed(_EngineID))
		{
			BEGIN_ISOLATE_SCOPE(_EngineProxy);
			BEGIN_CONTEXT_SCOPE(_EngineProxy);

			if (!_FunctionTemplate.IsEmpty())
				_FunctionTemplate.Reset();

			END_CONTEXT_SCOPE;
			END_ISOLATE_SCOPE;
		}

		_EngineProxy = nullptr;
	}
}

// ------------------------------------------------------------------------------------------------------------------------

void FunctionTemplateProxy::SetManagedCallback(ManagedJSFunctionCallback managedCallback) { _ManagedCallback = managedCallback; }

// ------------------------------------------------------------------------------------------------------------------------

void FunctionTemplateProxy::InvocationCallbackProxy(const FunctionCallbackInfo<Value>& args)
{
	auto proxy = (ProxyBase*)args.Data().As<External>()->Value();

	V8EngineProxy *engine;
	ManagedJSFunctionCallback callback;

	if (proxy->GetType() == FunctionTemplateProxyClass)
	{
		engine = ((FunctionTemplateProxy*)proxy)->_EngineProxy;
		callback = ((FunctionTemplateProxy*)proxy)->_ManagedCallback;
	}
	else if (proxy->GetType() == ObjectTemplateProxyClass)
	{
		engine = ((ObjectTemplateProxy*)proxy)->_EngineProxy;
		callback = ((ObjectTemplateProxy*)proxy)->_ManagedCallback;
	}
	else throw runtime_error("'args.Data()' is not recognized.");

	if (callback != nullptr) // (note: '_ManagedCallback' may not be set on the proxy, and thus 'callback' may be null)
	{
		auto argLength = args.Length();
		auto _args = argLength > 0 ? new HandleProxy*[argLength] : nullptr;

		for (auto i = 0; i < argLength; i++)
			_args[i] = engine->GetHandleProxy(args[i]);

		auto _this = engine->GetHandleProxy(args.This()); // (was args.Holder())

		engine->_InCallbackScope++;
		HandleProxy* result = nullptr;
		try {
			result = callback(0, args.IsConstructCall(), _this, _args, argLength);
		}
		catch (...) { ThrowException(NewString("'InvocationCallbackProxy' caused an error - perhaps the GC collected the delegate?").FromMaybe(Local<String>())); }
		engine->_InCallbackScope--;

		if (result != nullptr) {
			if (result->IsError()) {
				args.GetReturnValue().Set(ThrowException(Exception::Error(result->Handle()->ToString(engine->Context()).FromMaybe(Local<String>()))));
			}
			else {
				args.GetReturnValue().Set(result->Handle()); // (note: the returned value was created via p/invoke calls from the managed side, so the managed side is expected to tracked and free this handle when done)
			}
			result->TryDispose();		
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------

ObjectTemplateProxy* FunctionTemplateProxy::GetInstanceTemplateProxy()
{
	return _InstanceTemplate;
}

// ------------------------------------------------------------------------------------------------------------------------

ObjectTemplateProxy* FunctionTemplateProxy::GetPrototypeTemplateProxy()
{
	return _PrototypeTemplate;
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy* FunctionTemplateProxy::GetFunction()
{
	auto objML = _FunctionTemplate->GetFunction(_EngineProxy->Context());
	Local<Function> objL;
	if (!objML.ToLocal(&objL))
		return nullptr;

	auto proxyVal = _EngineProxy->GetHandleProxy(objL);
	return proxyVal;
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy* FunctionTemplateProxy::CreateInstance(int32_t managedObjectID, int32_t argCount, HandleProxy** args)
{
	Handle<Value>* hArgs = new Handle<Value>[argCount];
	for (int i = 0; i < argCount; i++)
		hArgs[i] = args[i]->Handle();

	auto funcML = _FunctionTemplate->GetFunction(_EngineProxy->Context());
	Local<Function> funcL;
	if (!funcML.ToLocal(&funcL))
		return nullptr;

	auto objML = funcL->NewInstance(_EngineProxy->Context(), argCount, hArgs);
	Local<Object> objL;
	if (!objML.ToLocal(&objL))
		return nullptr;

	delete[] hArgs; // TODO: (does "disposed" still need to be called here for each item?)

	if (managedObjectID == -1)
		managedObjectID = _EngineProxy->GetNextNonTemplateObjectID();

	auto proxyVal = _EngineProxy->GetHandleProxy(objL);
	proxyVal->_ObjectID = managedObjectID;
	//??auto count = obj->InternalFieldCount();
	objL->SetAlignedPointerInInternalField(0, this); // (stored a reference to the proxy instance for the call-back functions)
	objL->SetInternalField(1, NewExternal((void*)(int64_t)managedObjectID)); // (stored a reference to the managed object for the call-back functions)
	objL->SetPrivate(_EngineProxy->Context(), NewPrivateString("ManagedObjectID"), NewInteger(managedObjectID)); // (won't be used on template created objects [fields are faster], but done anyhow for consistency)
	return proxyVal;
}

// ------------------------------------------------------------------------------------------------------------------------

void FunctionTemplateProxy::Set(const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attributes)
{
	if (value != nullptr) {
		_FunctionTemplate->Set(ToLocalThrow(NewUString(name)), value->Handle(), attributes);  // TODO: Check how this affects objects created from templates!
	}
}

// ------------------------------------------------------------------------------------------------------------------------
