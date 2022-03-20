#include "ProxyTypes.h"

// ############################################################################################################################
// Misc. Global Functions

// ...

// ############################################################################################################################
// DLL Exports

// Prevent name mangling for the interface functions. 
extern "C"
{
	// ------------------------------------------------------------------------------------------------------------------------
	// Engine Related

	EXPORT V8EngineProxy* STDCALL CreateV8EngineProxy(bool enableDebugging, DebugMessageDispatcher *debugMessageDispatcher, int debugPort)
	{
		return new V8EngineProxy(enableDebugging, debugMessageDispatcher, debugPort);
	}
	EXPORT void STDCALL DestroyV8EngineProxy(V8EngineProxy *engine) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		delete engine;
	}

	EXPORT ContextProxy* STDCALL CreateContext(V8EngineProxy *engine, ObjectTemplateProxy *templatePoxy) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		return engine->CreateContext(templatePoxy);
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL DeleteContext(ContextProxy* context)
	{
		if (context != nullptr)
		{
			auto engine = context->EngineProxy();
			if (engine == nullptr) return; // (might have been destroyed)
			BEGIN_ISOLATE_SCOPE(engine);
			delete context;
			END_ISOLATE_SCOPE;
		}
	}

	EXPORT HandleProxy* STDCALL SetContext(V8EngineProxy *engine, ContextProxy* context) // (returns the global object handle)
	{
		BEGIN_ISOLATE_SCOPE(engine);
		return engine->SetContext(context);
		END_ISOLATE_SCOPE;
	}

	EXPORT ContextProxy* STDCALL GetContext(V8EngineProxy *engine) // (returns the global object handle)
	{
		BEGIN_ISOLATE_SCOPE(engine);
		return engine->GetContext();
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetFlagsFromString(V8EngineProxy *engine, const char *flags) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		if (flags != nullptr && strlen(flags) > 0)
			V8::SetFlagsFromString(flags, (int)strlen(flags));
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL RegisterGCCallback(V8EngineProxy* engine, ManagedV8GarbageCollectionRequestCallback managedV8GarbageCollectionRequestCallback) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		engine->RegisterGCCallback(managedV8GarbageCollectionRequestCallback);
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL ForceGC(V8EngineProxy* engine) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		engine->ProcessHandleQueues(1000);
		engine->Isolate()->LowMemoryNotification();
		while (!engine->Isolate()->IdleNotificationDeadline(1)) {}
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT bool STDCALL DoIdleNotification(V8EngineProxy* engine, int hint = 1) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		if (engine->IsExecutingScript()) return false;
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		engine->ProcessHandleQueues(1000);
		return engine->Isolate()->IdleNotificationDeadline(hint);
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL V8Execute(V8EngineProxy *engine, uint16_t *script, uint16_t *sourceName) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return engine->Execute(script, sourceName);
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	EXPORT HandleProxy* STDCALL V8Compile(V8EngineProxy *engine, uint16_t *script, uint16_t *sourceName) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return engine->Compile(script, sourceName);
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	EXPORT HandleProxy* STDCALL V8ExecuteCompiledScript(V8EngineProxy *engine, HandleProxy* script) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		if (script == nullptr || !script->IsScript())
			return engine->CreateError("Not a valid script handle.", JSV_ExecutionError);
		auto h = engine->Execute(script->Script());
		script->TryDispose();
		return h;
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL TerminateExecution(V8EngineProxy *engine) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		engine->TerminateExecution();
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// Object Template Related

	EXPORT ObjectTemplateProxy* STDCALL CreateObjectTemplateProxy(V8EngineProxy *engine) // TODO: Consider NOT using pointers here - instead, use the ID of the engine!
	{
		BEGIN_ISOLATE_SCOPE(engine);
		return engine->CreateObjectTemplate();
		END_ISOLATE_SCOPE;
	}
	EXPORT bool STDCALL DeleteObjectTemplateProxy(ObjectTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)
		if (engine->IsExecutingScript())
			return false; // TODO: Consider queuing this also.
		BEGIN_ISOLATE_SCOPE(engine);
		delete proxy;
		END_ISOLATE_SCOPE;
		return true;
	}

	EXPORT void STDCALL RegisterNamedPropertyHandlers(ObjectTemplateProxy *proxy,
		ManagedNamedPropertyGetter getter,
		ManagedNamedPropertySetter setter,
		ManagedNamedPropertyQuery query,
		ManagedNamedPropertyDeleter deleter,
		ManagedNamedPropertyEnumerator enumerator)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		proxy->RegisterNamedPropertyHandlers(getter, setter, query, deleter, enumerator);
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL RegisterIndexedPropertyHandlers(ObjectTemplateProxy *proxy,
		ManagedIndexedPropertyGetter getter,
		ManagedIndexedPropertySetter setter,
		ManagedIndexedPropertyQuery query,
		ManagedIndexedPropertyDeleter deleter,
		ManagedIndexedPropertyEnumerator enumerator)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		proxy->RegisterIndexedPropertyHandlers(getter, setter, query, deleter, enumerator);
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL UnregisterNamedPropertyHandlers(ObjectTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		proxy->UnregisterNamedPropertyHandlers();
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL UnregisterIndexedPropertyHandlers(ObjectTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		proxy->UnregisterIndexedPropertyHandlers();
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetCallAsFunctionHandler(ObjectTemplateProxy *proxy, ManagedJSFunctionCallback callback)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		proxy->SetCallAsFunctionHandler(callback);
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL CreateObjectFromTemplate(ObjectTemplateProxy *proxy, int32_t managedObjectID)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)

		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return proxy->CreateObject(managedObjectID);
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	// This function connects objects that are created internally by V8, but are based on custom templates (such as new instances created by functions where V8
	// creates the object internally and passes it along).
	// 'templateProxy' should be null (for basic non-template objects), or a reference to one of the native proxy template classes.
	EXPORT void STDCALL ConnectObject(HandleProxy *handleProxy, int32_t managedObjectID, void* templateProxy)
	{
		auto engine = handleProxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)

		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		if (managedObjectID == -1)
			managedObjectID = handleProxy->EngineProxy()->GetNextNonTemplateObjectID();

		auto handle = handleProxy->Handle();
		if (!handle.IsEmpty() && handle->IsObject())
		{
			auto obj = handleProxy->Handle().As<Object>();
			if (obj->InternalFieldCount() > 1) // (this is used on templates only, where a number of fields can bet set before objects are created [not possible otherwise]) 
			{
				if (templateProxy != nullptr)
					obj->SetAlignedPointerInInternalField(0, templateProxy); // (stored a reference to the proxy instance for the call-back function(s))
				obj->SetInternalField(1, NewExternal((void*)(int64_t)managedObjectID));
			}
			engine->SetObjectPrivateValue(obj, "ManagedObjectID", NewInteger(managedObjectID)); // (won't be used on template created objects [fields are faster], but done anyhow for consistency)
		}
		handleProxy->SetManagedObjectID(managedObjectID);

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL GetObjectPrototype(HandleProxy *handleProxy)
	{
		auto engine = handleProxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)

		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = handleProxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		return handleProxy->EngineProxy()->GetHandleProxy(handle.As<Object>()->GetPrototype());

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL Call(HandleProxy *subject, const uint16_t *functionName, HandleProxy *_this, uint16_t argCount, HandleProxy** args)
	{
		auto engine = subject->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
	
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto result = engine->Call(subject, functionName, _this, argCount, args);

		if (args != nullptr)
			for (int i = 0; i < argCount; ++i)
				if (args[i] != nullptr)
					args[i]->TryDispose();

		return result;

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	EXPORT bool STDCALL SetObjectPropertyByName(HandleProxy *proxy, const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attribs = v8::None)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)

		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();

		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");

		//? ... managed objects must have a clone of their handle set because it may be made weak by the worker if abandoned, and the handle lost ...
		//!Handle<Value> valueHandle = value == nullptr ? (Handle<Value>)V8Undefined : value->GetManagedObjectID() < 0 ? value->Handle() : value->Handle()->ToObject();
		Handle<Value> valueHandle = value != nullptr ? value->Handle() : (Handle<Value>)V8Undefined;

		if (value != nullptr)
			value->TryDispose();

		auto nameML = NewUString(name);
		Local<String> nameL;
		if (!nameML.ToLocal(&nameL))
			return false;

		auto obj = handle.As<Object>();
		return obj->DefineOwnProperty(engine->Context(), nameL, valueHandle, attribs).FromMaybe(false);

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT bool STDCALL SetObjectPropertyByIndex(HandleProxy *proxy, const uint32_t index, HandleProxy *value, v8::PropertyAttribute attribs = v8::None)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)

		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();

		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");

		auto obj = handle.As<Object>();

		//!auto valueHandle = new CopyablePersistent<Value>(value != nullptr ? value->Handle() : V8Undefined);
		Handle<Value> valueHandle = value != nullptr ? value->Handle() : (Handle<Value>)V8Undefined;

		if (value != nullptr)
			value->TryDispose();

		if (attribs == 0)
			return obj->Set(engine->Context(), index, valueHandle).FromMaybe(false);
		else
		{
			MaybeLocal<String> nameML = Int32::New(engine->Isolate(), index)->ToString(engine->Context());
			Local<String> nameL;
			if (!nameML.ToLocal(&nameL))
				return false;
			return obj->DefineOwnProperty(engine->Context(), nameL, valueHandle, attribs).FromMaybe(false);
		}

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL GetObjectPropertyByName(HandleProxy *proxy, const uint16_t *name)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		auto propML = obj->Get(engine->Context(), ToLocalThrow(NewUString(name)));
		Local<Value> propL;
		if (!propML.ToLocal(&propL))
			return nullptr;
		return engine->GetHandleProxy(propL);

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL GetObjectPropertyByIndex(HandleProxy *proxy, const uint32_t index)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		auto propML = obj->Get(engine->Context(), index);
		Local<Value> propL;
		if (!propML.ToLocal(&propL))
			return nullptr;
		return engine->GetHandleProxy(propL);

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT bool STDCALL DeleteObjectPropertyByName(HandleProxy *proxy, const uint16_t *name)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		auto nameML = NewUString(name);
		Local<String> nameL;
		if (!nameML.ToLocal(&nameL))
			return false;

		return ToThrow(obj->Delete(engine->Context(), nameL));

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT bool STDCALL DeleteObjectPropertyByIndex(HandleProxy *proxy, const uint32_t index)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();
		return ToThrow(obj->Delete(engine->Context(), index));

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetObjectAccessor(HandleProxy *proxy, int32_t managedObjectID, const uint16_t *name,
		ManagedAccessorGetter getter, ManagedAccessorSetter setter,
		v8::AccessControl access, v8::PropertyAttribute attributes)
	{
		if (attributes < 0) // (-1 is "No Access" on the managed side, but there is no native support in V8 for this)
			attributes = (PropertyAttribute)(ReadOnly | DontEnum);

		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");

		auto obj = handle.As<Object>();

		engine->SetObjectPrivateValue(obj, "ManagedObjectID", NewInteger(managedObjectID));

		auto accessors = NewArray(3); // [0] == ManagedObjectID, [1] == getter, [2] == setter
		if (!accessors->Set(engine->Context(), 0, NewInteger(managedObjectID)).FromMaybe(false))
			throw runtime_error("accessor[0]: setting managedObjectID failed.");

		if (!accessors->Set(engine->Context(), 1, NewExternal((void*)getter)).FromMaybe(false))
			throw runtime_error("accessor[1]: setting getter failed.");

		if (!accessors->Set(engine->Context(), 2, NewExternal((void*)setter)).FromMaybe(false))
			throw runtime_error("accessor[2]: setting setter failed.");

		Local<String> nameL = ToLocalThrow(NewUString(name));
		obj->Delete(engine->Context(), nameL); //? ForceDelete()?

		// TODO: Check how this affects objects created from templates!
		if (!obj->SetAccessor(engine->Context(), nameL, ObjectTemplateProxy::AccessorGetterCallbackProxy, ObjectTemplateProxy::AccessorSetterCallbackProxy, accessors, access, attributes).FromMaybe(false))
			throw runtime_error("obj->SetAccessor failed.");

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetObjectTemplateAccessor(ObjectTemplateProxy *proxy, int32_t managedObjectID, const uint16_t *name,
		ManagedAccessorGetter getter, ManagedAccessorSetter setter,
		v8::AccessControl access, v8::PropertyAttribute attributes)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		proxy->SetAccessor(managedObjectID, name, getter, setter, access, attributes);  // TODO: Check how this affects objects created from templates!

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetObjectTemplateProperty(ObjectTemplateProxy *proxy, const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attributes)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		proxy->Set(name, value, attributes);  // TODO: Check how this affects objects created from templates!

		if (value != nullptr)
			value->TryDispose();

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL GetPropertyNames(HandleProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		return proxy->EngineProxy()->GetHandleProxy(ToLocalThrow(obj->GetPropertyNames(engine->Context())));

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT HandleProxy* STDCALL GetOwnPropertyNames(HandleProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		return proxy->EngineProxy()->GetHandleProxy(ToLocalThrow(obj->GetOwnPropertyNames(engine->Context())));

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT PropertyAttribute STDCALL GetPropertyAttributes(HandleProxy *proxy, const uint16_t * name)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return (PropertyAttribute)0; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsObject())
			throw runtime_error("The handle does not represent an object.");
		auto obj = handle.As<Object>();

		PropertyAttribute attr;
		if (!obj->GetPropertyAttributes(engine->Context(), ToLocalThrow(NewUString(name))).To(&attr))
			throw runtime_error("GetPropertyAttributes failed.");

		return attr;

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT int32_t STDCALL GetArrayLength(HandleProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return 0; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);

		auto handle = proxy->Handle();
		if (handle.IsEmpty() || !handle->IsArray())
			throw runtime_error("The handle does not represent an array object.");
		return handle.As<Array>()->Length();

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// Function Template Related
	EXPORT FunctionTemplateProxy* STDCALL CreateFunctionTemplateProxy(V8EngineProxy *engine, uint16_t *className, ManagedJSFunctionCallback callback)
	{
		BEGIN_ISOLATE_SCOPE(engine);
		return engine->CreateFunctionTemplate(className, callback);
		END_ISOLATE_SCOPE;
	}
	EXPORT bool STDCALL DeleteFunctionTemplateProxy(FunctionTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return false; // (might have been destroyed)
		if (engine->IsExecutingScript())
			return false; // TODO: Consider queuing this also.
		BEGIN_ISOLATE_SCOPE(engine);
		delete proxy;
		END_ISOLATE_SCOPE;
		return true;
	}
	EXPORT ObjectTemplateProxy* STDCALL GetFunctionInstanceTemplateProxy(FunctionTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return proxy->GetInstanceTemplateProxy();
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	EXPORT ObjectTemplateProxy* STDCALL GetFunctionPrototypeTemplateProxy(FunctionTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return proxy->GetPrototypeTemplateProxy();
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	//??EXPORT void STDCALL SetManagedJSFunctionCallback(FunctionTemplateProxy *proxy, ManagedJSFunctionCallback callback)  { proxy->SetManagedCallback(callback); }

	EXPORT HandleProxy* STDCALL GetFunction(FunctionTemplateProxy *proxy)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		return proxy->GetFunction();
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	//??EXPORT HandleProxy* STDCALL GetFunctionPrototype(FunctionTemplateProxy *proxy, int32_t managedObjectID, ObjectTemplateProxy *objTemplate)
	//??{ return proxy->GetPrototype(managedObjectID, objTemplate); }
	EXPORT HandleProxy* STDCALL CreateInstanceFromFunctionTemplate(FunctionTemplateProxy *proxy, int32_t managedObjectID, int32_t argCount = 0, HandleProxy** args = nullptr)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return nullptr; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		auto result = proxy->CreateInstance(managedObjectID, argCount, args);

		if (args != nullptr)
			for (int i = 0; i < argCount; ++i)
				if (args[i] != nullptr)
					args[i]->TryDispose();

		return result;

		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	EXPORT void STDCALL SetFunctionTemplateProperty(FunctionTemplateProxy *proxy, const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attributes)
	{
		auto engine = proxy->EngineProxy();
		if (engine == nullptr) return; // (might have been destroyed)
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		proxy->Set(name, value, attributes);  // TODO: Check how this affects objects created from templates!
		if (value != nullptr)
			value->TryDispose();
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// Value Creation 

	EXPORT HandleProxy* STDCALL CreateBoolean(V8EngineProxy *engine, bool b) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateBoolean(b); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateInteger(V8EngineProxy *engine, int32_t num) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateInteger(num); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateNumber(V8EngineProxy *engine, double num) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateNumber(num); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateString(V8EngineProxy *engine, uint16_t* str) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateString(str); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateDate(V8EngineProxy *engine, double ms) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateDate(ms); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateObject(V8EngineProxy *engine, int32_t managedObjectID) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateObject(managedObjectID); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }
	EXPORT HandleProxy* STDCALL CreateArray(V8EngineProxy *engine, HandleProxy** items, uint16_t length)
	{
		BEGIN_ISOLATE_SCOPE(engine);
		BEGIN_CONTEXT_SCOPE(engine);
		auto a = engine->CreateArray(items, length);

		if (items != nullptr)
			for (int i = 0; i < length; ++i)
				if (items[i] != nullptr)
					items[i]->TryDispose();

		return a;
		END_CONTEXT_SCOPE;
		END_ISOLATE_SCOPE;
	}
	EXPORT HandleProxy* STDCALL CreateStringArray(V8EngineProxy *engine, uint16_t **items, uint16_t length) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateArray(items, length); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }

	EXPORT HandleProxy* STDCALL CreateNullValue(V8EngineProxy *engine) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateNullValue(); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }

	EXPORT HandleProxy* STDCALL CreateError(V8EngineProxy *engine, uint16_t* message, JSValueType errorType) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->CreateError(message, errorType); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }

	//EXPORT void STDCALL RequestGarbageCollectionForTesting(V8EngineProxy *engine) { BEGIN_ISOLATE_SCOPE(engine); BEGIN_CONTEXT_SCOPE(engine); return engine->RequestGarbageCollectionForTesting(GarbageCollectionType::kFullGarbageCollection); END_CONTEXT_SCOPE; END_ISOLATE_SCOPE; }

	// ------------------------------------------------------------------------------------------------------------------------
	// Handle Related

	EXPORT void STDCALL MakeWeakHandle(HandleProxy *handleProxy)
	{
		if (handleProxy != nullptr)
		{
			auto engine = handleProxy->EngineProxy();
			if (engine == nullptr) return; // (might have been destroyed)

			//if (engine->IsExecutingScript()) // TODO: Better to use thread detection perhaps...?
			//{
			//	// ... a script is running, so have 'GetHandleProxy()' take some responsibility to check a queue ...
			//	engine->QueueMakeWeak(handleProxy); // TODO: Not sure if this is still needed...?
			//}
			//else
			//{
			BEGIN_ISOLATE_SCOPE(engine);
			BEGIN_CONTEXT_SCOPE(engine);
			handleProxy->MakeWeak();
			END_CONTEXT_SCOPE;
			END_ISOLATE_SCOPE;
			//}
		}
	}
	EXPORT void STDCALL MakeStrongHandle(HandleProxy *handleProxy)
	{
		if (handleProxy != nullptr)
		{
			auto engine = handleProxy->EngineProxy();
			if (engine == nullptr) return; // (might have been destroyed)

			//if (engine->IsExecutingScript())
			//{
			//	// ... a script is running, so have 'GetHandleProxy()' take some responsibility to check a queue ...
			//	engine->QueueMakeStrong(handleProxy);
			//}
			//else
			//{
			BEGIN_ISOLATE_SCOPE(engine);
			BEGIN_CONTEXT_SCOPE(engine);
			handleProxy->MakeStrong();
			END_CONTEXT_SCOPE;
			END_ISOLATE_SCOPE;
			//}
		}
	}

	EXPORT void STDCALL DisposeHandleProxy(HandleProxy *handleProxy)
	{
		if (handleProxy != nullptr)
		{
			auto engine = handleProxy->EngineProxy();
			if (engine != nullptr) // (might have been destroyed)
				if (engine->IsExecutingScript())
				{
					// ... a script is running, so make it weak so the GC collects this later (if a script is running calling this will queue it up)...
					engine->QueueHandleDisposal(handleProxy); // TODO: Create a queue for disposing handles as well.
				}
				else
				{
					BEGIN_ISOLATE_SCOPE(engine);
					BEGIN_CONTEXT_SCOPE(engine);
					handleProxy->Dispose();
					END_CONTEXT_SCOPE;
					END_ISOLATE_SCOPE;
				}
		}
	}

	EXPORT void STDCALL UpdateHandleValue(HandleProxy *handleProxy)
	{
		if (handleProxy != nullptr)
		{
			auto engine = handleProxy->EngineProxy();
			if (engine == nullptr) return; // (might have been destroyed)
			BEGIN_ISOLATE_SCOPE(engine);
			BEGIN_CONTEXT_SCOPE(engine);
			handleProxy->UpdateValue();
			END_CONTEXT_SCOPE;
			END_ISOLATE_SCOPE;
		}
	}
	EXPORT int STDCALL GetHandleManagedObjectID(HandleProxy *handleProxy)
	{
		if (handleProxy != nullptr)
		{
			auto engine = handleProxy->EngineProxy();
			if (engine == nullptr) return -1; // (might have been destroyed)
			BEGIN_ISOLATE_SCOPE(engine);
			BEGIN_CONTEXT_SCOPE(engine);
			return handleProxy->GetManagedObjectID();
			END_CONTEXT_SCOPE;
			END_ISOLATE_SCOPE;
		}
		else return -2;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	EXPORT HandleProxy* STDCALL CreateHandleProxyTest()
	{
		byte* data = new byte[sizeof(HandleProxy)];
		for (int i = 0; i < sizeof(HandleProxy); i++)
			data[i] = i;
		TProxyObjectType* pType = (TProxyObjectType*)data;
		*pType = HandleProxyClass;
		return reinterpret_cast<HandleProxy*>(data);
	}

	EXPORT V8EngineProxy* STDCALL CreateV8EngineProxyTest()
	{
		byte* data = new byte[sizeof(V8EngineProxy)];
		for (int i = 0; i < sizeof(V8EngineProxy); i++)
			data[i] = i;
		TProxyObjectType* pType = (TProxyObjectType*)data;
		*pType = V8EngineProxyClass;
		return reinterpret_cast<V8EngineProxy*>(data);
	}

	EXPORT ObjectTemplateProxy* STDCALL CreateObjectTemplateProxyTest()
	{
		byte* data = new byte[sizeof(ObjectTemplateProxy)];
		for (int i = 0; i < sizeof(ObjectTemplateProxy); i++)
			data[i] = i;
		TProxyObjectType* pType = (TProxyObjectType*)data;
		*pType = ObjectTemplateProxyClass;
		return reinterpret_cast<ObjectTemplateProxy*>(data);
	}

	EXPORT FunctionTemplateProxy* STDCALL CreateFunctionTemplateProxyTest()
	{
		byte* data = new byte[sizeof(FunctionTemplateProxy)];
		for (int i = 0; i < sizeof(FunctionTemplateProxy); i++)
			data[i] = i;
		TProxyObjectType* pType = (TProxyObjectType*)data;
		*pType = FunctionTemplateProxyClass;
		return reinterpret_cast<FunctionTemplateProxy*>(data);
	}

	EXPORT void STDCALL DeleteTestData(byte* data)
	{
		ProxyBase* pBase = reinterpret_cast<ProxyBase*>(data);
		if (pBase->GetType() == ObjectTemplateProxyClass)
		{
			memset(data, 0, sizeof(ObjectTemplateProxy));
			delete[] data;
		}
		else if (pBase->GetType() == FunctionTemplateProxyClass)
		{
			memset(data, 0, sizeof(FunctionTemplateProxy));
			delete[] data;
		}
		else if (pBase->GetType() == V8EngineProxyClass)
		{
			memset(data, 0, sizeof(V8EngineProxy));
			delete[] data;
		}
		else if (pBase->GetType() == HandleProxyClass)
		{
			memset(data, 0, sizeof(HandleProxy));
			delete[] data;
		}
		else
		{
			throw runtime_error("'Data' points to an invalid object reference and cannot be deleted.");
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------
}

// ############################################################################################################################
