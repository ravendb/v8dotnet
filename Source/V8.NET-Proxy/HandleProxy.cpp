#include "ProxyTypes.h"
#include <string.h>

// ------------------------------------------------------------------------------------------------------------------------

v8::Local<Value> HandleProxy::Handle() { return _Handle; }
v8::Local<Script> HandleProxy::Script() { return _Script; }

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy::HandleProxy(V8EngineProxy* engineProxy, int32_t id)
	: ProxyBase(HandleProxyClass), _Type((JSValueType)-1), _ID(id), _ManagedReference(0), _ObjectID(-1), _CLRTypeID(-1), __EngineProxy(0), _Disposed(0)
{
	_EngineProxy = engineProxy;
	_EngineID = _EngineProxy->_EngineID;
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy::~HandleProxy()
{
	if (Type != 0) // (type is 0 if this class was wiped with 0's {if used in a marshalling test})
	{
		_ClearHandleValue();
		_ObjectID = -1;
		_Disposed = 3;
		_ManagedReference = 0;
	}
}

V8EngineProxy* HandleProxy::EngineProxy() { return _EngineID >= 0 && !V8EngineProxy::IsDisposed(_EngineID) ? _EngineProxy : nullptr; }


// Sets the state if this instance to disposed (for safety, the handle is NOT deleted, only cached).
// (registerDisposal is false when called within 'V8EngineProxy.DisposeHandleProxy()' (to prevent a cyclical loop), or by the engine's destructor)
bool HandleProxy::_Dispose(bool registerDisposal)
{
	if (IsDisposed()) return true; // (already disposed)
	if (V8EngineProxy::IsDisposed(_EngineID))
	{
		delete this; // (the engine is gone, so just destroy the memory [the managed side owns UNDISPOSED proxy handles - they are not deleted with the engine)
		return false; // (already disposed, or engine is gone)
	}
	else {
		lock_guard<recursive_mutex>(_EngineProxy->_HandleSystemMutex); // NO V8 HANDLE ACCESS HERE BECAUSE OF THE MANAGED GC

		if (!IsDisposed() && IsDisposeReadyManagedSide())
		{
			if (registerDisposal)
			{
				_EngineProxy->DisposeHandleProxy(this); // (calls back with _Dispose(false); NOTE: REQUIRES '_ID' and '_ObjectID', so don't clear before this)
				return true;
			}

			_Disposed = 3; // (just to be consistent)

			_ClearHandleValue();

			_ObjectID = -1;
			_CLRTypeID = -1;
			_ManagedReference = 0;
			_Type = JSV_Uninitialized;
			_EngineID = -1;
			_EngineProxy = nullptr;
		};
	}
	return true;
}

bool HandleProxy::Dispose()
{
	return _Dispose(true);
}

bool HandleProxy::TryDispose()
{
	if (IsDisposeReadyManagedSide())
	{
		return Dispose();
	}
	return false; // (already disposed, or if _ManagedReference == 2, the user called 'KeepAlive()' or 'Set()' on the handle [makes it tracked])
}

// ------------------------------------------------------------------------------------------------------------------------

HandleProxy* HandleProxy::Initialize(v8::Handle<Value> handle)
{
	if (_Disposed != 0)
		if (!_Dispose(false)) return nullptr; // (just resets whatever is needed)

	_Disposed = 0; // (MUST do this FIRST in order for any associated managed object ID to be pulled, otherwise it will remain -1)

	SetHandle(handle);

	return this;
}

// ------------------------------------------------------------------------------------------------------------------------

void HandleProxy::_ClearHandleValue()
{
	if (!_Handle.IsEmpty())
	{
		_Handle.Reset();
	}
	if (!_Script.IsEmpty())
	{
		_Script.Reset();
	}
	_Value.Dispose();
	_Type = JSV_Uninitialized;
	if (_Handle.IsWeak())
		throw runtime_error("HandleProxy::_ClearHandleValue(): Assertion failed - tried to clear a handle that is still in a weak state.");
}

HandleProxy* HandleProxy::SetHandle(v8::Handle<v8::Script> handle)
{
	_ClearHandleValue();

	_Script = CopyablePersistent<v8::Script>(handle);
	_Type = JSV_Script;

	return this;
}

HandleProxy* HandleProxy::SetHandle(v8::Handle<Value> handle)
{
	_ClearHandleValue();

	_Handle = CopyablePersistent<Value>(handle);

	if (_Handle.IsEmpty())
	{
		_Type = JSV_Undefined;
	}
	else if (_Handle->IsBoolean())
	{
		_Type = JSV_Bool;
	}
	else if (_Handle->IsBooleanObject()) // TODO: Validate this is correct.
	{
		_Type = JSV_BoolObject;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsInt32())
	{
		_Type = JSV_Int32;
	}
	else if (_Handle->IsNumber())
	{
		_Type = JSV_Number;
	}
	else if (_Handle->IsNumberObject()) // TODO: Validate this is correct.
	{
		_Type = JSV_NumberObject;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsString())
	{
		_Type = JSV_String;
	}
	else if (_Handle->IsStringObject())// TODO: Validate this is correct.
	{
		_Type = JSV_StringObject;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsDate())
	{
		_Type = JSV_Date;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsArray())
	{
		_Type = JSV_Array;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsRegExp())
	{
		_Type = JSV_RegExp;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsNull())
	{
		_Type = JSV_Null;
	}
	else if (_Handle->IsFunction())
	{
		_Type = JSV_Function;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsExternal())
	{
		_Type = JSV_Undefined;
	}
	else if (_Handle->IsNativeError())
	{
		_Type = JSV_Undefined;
	}
	else if (_Handle->IsUndefined())
	{
		_Type = JSV_Undefined;
	}
	else if (_Handle->IsObject()) // WARNING: Do this AFTER any possible object type checks (example: creating functions makes this return true as well!!!)
	{
		_Type = JSV_Object;
		GetManagedObjectID(); // (best to call this now for objects to prevent calling back into the native side again [also, prevents debugger errors when inspecting in a GC finalizer])
	}
	else if (_Handle->IsFalse()) // TODO: Validate this is correct.
	{
		_Type = JSV_Bool;
	}
	else if (_Handle->IsTrue()) // TODO: Validate this is correct.
	{
		_Type = JSV_Bool;
	}
	else
	{
		_Type = JSV_Undefined;
	}

	return this;
}

//void HandleProxy::_DisposeCallback(const WeakCallbackData<Value, HandleProxy>& data)
//{
	//auto engineProxy = (V8EngineProxy*)isolate->GetData();
	//auto handleProxy = parameter;
	//?object.Reset();
//}

// ------------------------------------------------------------------------------------------------------------------------

int32_t HandleProxy::SetManagedObjectID(int32_t id)
{
	// ... first, nullify any exiting mappings for the managed object ID ...
	if (_ObjectID >= (int32_t)0 && _ObjectID < (int32_t)_EngineProxy->_Objects.size())
		_EngineProxy->_Objects[_ObjectID] = nullptr;

	_ObjectID = id;

	if (_ObjectID >= 0)
	{
		// ... store a mapping from managed object ID to this handle proxy ...
		if (_ObjectID >= (int32_t)_EngineProxy->_Objects.size())
			_EngineProxy->_Objects.resize((_ObjectID + 100) * 2, nullptr);

		_EngineProxy->_Objects[_ObjectID] = this;
	}
	else if (_ObjectID == -1)
		_ObjectID = _EngineProxy->GetNextNonTemplateObjectID(); // (must return something to associate accessor delegates, etc.)

	// ... detect if this is a special "type" object ...
	if (_ObjectID < -2 && _Handle->IsObject())
	{
		// ... use "duck typing" to determine if the handle is a valid TypeInfo object ...
		auto obj = _Handle.As<Object>();

		auto hTypeIdML = obj->Get(_EngineProxy->Context(), ToLocalThrow(NewString("$__TypeID")));
		Local<Value> hTypeIdL;
		if (!hTypeIdML.ToLocal(&hTypeIdL))
		{
			if (hTypeIdL->IsNumber() || hTypeIdL->IsInt32())
			{
				int32_t value = hTypeIdL->IsNumber() ? hTypeIdL->Int32Value(_EngineProxy->Context()).FromJust() : (int32_t)hTypeIdL->NumberValue(_EngineProxy->Context()).FromJust();
				if (obj->Has(_EngineProxy->Context(), ToLocalThrow(NewString("$__Value"))).FromMaybe(false))
				{
					_CLRTypeID = value;
				}
			}
		}
	}

	return _ObjectID;
}

// Should be called once to attempt to pull the ID.
// If there's no ID, then the managed object ID will be set to -2 to prevent checking again.
// To force a re-check, simply set the value back to -1.
int32_t HandleProxy::GetManagedObjectID()
{
	if (IsDisposed())
		return -1; // (no longer in use!)
	else if (_ObjectID < -1 || _ObjectID >= 0)
		return _ObjectID;
	else
		return SetManagedObjectID(HandleProxy::GetManagedObjectID(_Handle));
}


// If the given handle is an object, this will attempt to pull the managed side object ID, or -1 otherwise.
int32_t HandleProxy::GetManagedObjectID(v8::Handle<Value> h)
{
	int32_t id = -1;

	if (!h.IsEmpty() && h->IsObject())
	{
		// ... if this was created by a template then there will be at least 2 fields set, so assume the second is a managed ID value, 
		// but if not, then check for a hidden property for objects not created by templates ...

		auto obj = h.As<Object>();

		if (obj->InternalFieldCount() > 1)
		{
			auto field = obj->GetInternalField(1); // (may be faster than hidden values)
			if (field->IsExternal())
				id = (int32_t)(int64_t)field.As<External>()->Value();
		}
		else
		{
			auto priv_sym = Private::ForApi(Isolate::GetCurrent(), ToLocalThrow(NewString("$ManagedObjectID"))); // TODO: Better way to do this?
			auto handleML = obj->GetPrivate(Isolate::GetCurrent()->GetEnteredOrMicrotaskContext(), priv_sym);
			Local<Value> handleL;
			if (!handleML.ToLocal(&handleL))
			{
				if (!handleL.IsEmpty() && handleL->IsInt32())
					handleL->Int32Value(Isolate::GetCurrent()->GetEnteredOrMicrotaskContext()).To(&id);
			}
		}
	}

	return id;
}

// ------------------------------------------------------------------------------------------------------------------------

// This is called when the managed side is ready to destroy the V8 handle.
void HandleProxy::MakeWeak()
{
	if (!_Handle.IsEmpty())
		_Handle.Value.SetWeak<HandleProxy>(this, _RevivableCallback, WeakCallbackType::kFinalizer);
}

// This is called when the managed side is no longer ready to destroy this V8 handle.
void HandleProxy::MakeStrong()
{
	if (!_Handle.IsEmpty())
		_Handle.Value.ClearWeak();
}

// ------------------------------------------------------------------------------------------------------------------------

// When the managed side is ready to destroy a handle, it first marks it as weak.  When the V8 engine's garbage collector finally calls back, the managed side
// object information is finally destroyed.
void HandleProxy::_RevivableCallback(const WeakCallbackInfo<HandleProxy>& data)
{
	auto engineProxy = (V8EngineProxy*)data.GetIsolate()->GetData(0);
	auto handleProxy = data.GetParameter();

	//auto dispose = true;

	engineProxy->_InCallbackScope++;
	auto canDisposeNow = handleProxy->IsDisposeReadyManagedSide() || engineProxy->_ManagedV8GarbageCollectionRequestCallback != nullptr && engineProxy->_ManagedV8GarbageCollectionRequestCallback(handleProxy);
	//auto canDisposeNow = handleProxy->IsDisposeReadyManagedSide() || engineProxy->_ManagedV8GarbageCollectionRequestCallback != nullptr && engineProxy->_ManagedV8GarbageCollectionRequestCallback(engineProxy->GetContext(), handleProxy);  // TODO [shlomo] for moving _Objects to context
	engineProxy->_InCallbackScope--;

	if (canDisposeNow) // (if the managed side is ok with it, we will clear and dispose this handle now)
	{
		handleProxy->_ClearHandleValue();
		handleProxy->Dispose();
	}
	else handleProxy->MakeStrong();

	//if (engineProxy->_ManagedV8GarbageCollectionRequestCallback != nullptr)
	//{
	//	if (handleProxy->_ObjectID >= 0)
	//		dispose = engineProxy->_ManagedV8GarbageCollectionRequestCallback(handleProxy);
	//}

	//if (dispose) // (Note: the managed callback may have already cached the handle, but the handle *value* will not be disposed yet)
	//{
	//	handleProxy->_ClearHandleValue();
	//	// (V8 handle is no longer tracked on the managed side, so let it go within this GC request [better here while idle])
	//}
}

// ------------------------------------------------------------------------------------------------------------------------

void HandleProxy::UpdateValue()
{
	if (_Type == JSV_Script) return;

	_Value.Dispose();

	switch (_Type)
	{
		// (note: if this doesn't indent properly in VS, then see 'Tools > Options > Text Editor > C/C++ > Formatting > Indentation > Indent case labels')
		_Value.V8String = _StringItem(_EngineProxy, *_Handle.As<String>()).String; // (note: string is not disposed by struct object and becomes owned by this proxy!)
		case JSV_Null:
		{
			_Value.V8Number = 0;
			break;
		}
		case JSV_Bool:
		{
			_Value.V8Boolean = _Handle->BooleanValue(_EngineProxy->Context()->GetIsolate());
			break;
		}
		case JSV_BoolObject:
		{
			_Value.V8Boolean = _Handle->BooleanValue(_EngineProxy->Context()->GetIsolate());
			break;
		}
		case JSV_Int32:
		{
			_Value.V8Integer = _Handle->Int32Value(_EngineProxy->Context()).FromJust();
			break;
		}
		case JSV_Number:
		{
			_Value.V8Number = _Handle->NumberValue(_EngineProxy->Context()).FromJust();
			break;
		}
		case JSV_NumberObject:
		{
			_Value.V8Number = _Handle->NumberValue(_EngineProxy->Context()).FromJust();
			break;
		}
		case JSV_ExecutionTerminated:
		case JSV_ExecutionError:
		case JSV_CompilerError:
		case JSV_InternalError:
		case JSV_String:
		{
			_Value.V8String = _StringItem(_EngineProxy, *_Handle.As<String>()).String; // (note: string is not disposed by struct object and becomes owned by this proxy!)
			break;
		}
		case JSV_StringObject:
		{
			_Value.V8String = _StringItem(_EngineProxy, *_Handle.As<String>()).String;
			break;
		}
		case JSV_Date:
		{
			_Value.V8Number = _Handle.As<Date>()->ValueOf(); // Date::Cast(*_Handle.Handle())->ValueOf(); //_Handle.As<Number>()->Value(); //_Handle->NumberValue(_EngineProxy->Context()).FromJust();
			//_Value.V8String = _StringItem().String; //_StringItem(_EngineProxy, *_Handle.As<String>()).String;
			break;
		}
		case JSV_Undefined:
		case JSV_Uninitialized:
		{
			_Value.V8Number = 0; // (make sure this is cleared just in case...)
			break;
		}
		default: // (by default, an "object" type is assumed (warning: this includes functions); however, we can't translate it (obviously), so we just return a reference to this handle proxy instead)
		{
			if (!_Handle.IsEmpty()) {
				auto resML = _Handle->ToString(_EngineProxy->Context());
				Local<String> resL;
				if (resML.ToLocal(&resL))
					_Value.V8String = _StringItem(_EngineProxy, *resL).String;
			}
			break;
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------------
