/*
    File: instance.cc
*/

/*
Copyright (c) 2014, Christian E. Schafmeister

CLASP is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

See directory 'clasp/licenses' for full details.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* -^- */
//#define DEBUG_LEVEL_FULL

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
//#pragma GCC diagnostic ignored "-Wunused-local-typedef"
#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#pragma clang diagnostic pop

#include <clasp/core/foundation.h>
#include <clasp/core/object.h>
#include <clasp/core/lisp.h>
#include <clasp/core/symbolTable.h>
#include <clasp/gctools/gcFunctions.h>
#include <clasp/core/serialize.h>
#include <clasp/core/lispList.h>
#include <clasp/core/hashTable.h>
#include <clasp/core/hashTableEq.h>
#include <clasp/core/evaluator.h>
#include <clasp/core/genericFunction.h>
#include <clasp/core/accessor.h>
#include <clasp/core/instance.h>
#include <clasp/core/wrappers.h>

namespace core {

CL_DEFUN T_mv clos__getFuncallableInstanceFunction(T_sp obj) {
  if (Instance_sp iobj = obj.asOrNull<Instance_O>()) {
    switch (iobj->_isgf) {
    case CLASP_STANDARD_DISPATCH:
        return Values(_lisp->_true(),Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_RESTRICTED_DISPATCH:
        return Values(cl::_sym_standardGenericFunction,Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_READER_DISPATCH:
        return Values(clos::_sym_standardOptimizedReaderMethod,Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_WRITER_DISPATCH:
        return Values(clos::_sym_standardOptimizedWriterMethod,Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_USER_DISPATCH:
        return Values(iobj->userFuncallableInstanceFunction(),Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_STRANDH_DISPATCH:
        return Values(iobj->GFUN_DISPATCHER(),Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_INVALIDATED_DISPATCH:
        return Values(clos::_sym_invalidated_dispatch_function,Pointer_O::create((void*)iobj->_entryPoint));
    case CLASP_NOT_FUNCALLABLE:
        return Values(clos::_sym_not_funcallable);
    }
    return Values(clasp_make_fixnum(iobj->_isgf),_Nil<T_O>());
  }
  return Values(_Nil<T_O>(),_Nil<T_O>());
};

CL_DEFUN T_sp clos__setFuncallableInstanceFunction(T_sp obj, T_sp func) {
  if (Instance_sp iobj = obj.asOrNull<Instance_O>()) {
    return iobj->setFuncallableInstanceFunction(func);
  }
  SIMPLE_ERROR(BF("You can only setFuncallableInstanceFunction on instances - you tried to set it on a: %s") % _rep_(obj));
};

CL_LAMBDA(instance func);
CL_DECLARE();
CL_DOCSTRING("instanceClassSet");
CL_DEFUN T_sp core__instance_class_set(T_sp obj, Class_sp mc) {
  if (Instance_sp iobj = obj.asOrNull<Instance_O>()) {
    return iobj->instanceClassSet(mc);
  }
  SIMPLE_ERROR(BF("You can only instanceClassSet on Instance_O or Class_O - you tried to set it on a: %s") % _rep_(mc));
};

CL_LAMBDA(obj);
CL_DECLARE();
CL_DOCSTRING("copy-instance returns a shallow copy of the instance");
CL_DEFUN T_sp core__copy_instance(T_sp obj) {
  if (gc::IsA<Instance_sp>(obj)) {
    Instance_sp iobj = gc::As_unsafe<Instance_sp>(obj);
    Instance_sp cp = iobj->copyInstance();
    return cp;
  } else if (gc::IsA<Class_sp>(obj)) {
    Class_sp cobj = gc::As_unsafe<Class_sp>(obj);
    Class_sp cp = cobj->copyInstance();
    return cp;
  }
  SIMPLE_ERROR(BF("copy-instance doesn't support copying %s") % _rep_(obj));
};

void Instance_O::GFUN_CALL_HISTORY_set(T_sp h) {
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   GFUN_CALL_HISTORY_set gf: %s\n", __FILE__, __LINE__, this->__repr__().c_str());
    printf("%s:%d                      history: %s\n", __FILE__, __LINE__, _rep_(h).c_str());
  }
#endif
  this->instanceSet(4,h);
}

void Instance_O::set_kind(Symbol_sp k) {
  if (k == kw::_sym_macro) {
    SIMPLE_ERROR(BF("You cannot set a generic-function (instance) to macro"));
  }
}

void Instance_O::initializeSlots(gctools::Stamp stamp, size_t numberOfSlots) {
  this->_Rack = SimpleVector_O::make(numberOfSlots+1,_Unbound<T_O>(),true);
  this->stamp_set(stamp);
#ifdef DEBUG_GUARD_VALIDATE
  client_validate(this->_Rack);
#endif
//  printf("%s:%d  Make sure you initialize slots for classes this->_Class -> %s\n", __FILE__, __LINE__, _rep_(this->_Class).c_str());
}

void Instance_O::initializeClassSlots(Creator_sp creator, gctools::Stamp stamp) {
  // Initialize slots they way they are in +class-slots+ in: https://github.com/drmeister/clasp/blob/dev-class/src/lisp/kernel/clos/hierarchy.lsp#L55
#if 0
  if (creator.unboundp()) {
    printf("%s:%d:%s     creator for %s is unbound\n", __FILE__, __LINE__, __FUNCTION__, this->_classNameAsString().c_str() );
    fflush(stdout);
  }
#endif
  this->instanceSet(REF_CLASS_DIRECT_SUPERCLASSES, _Nil<T_O>());
  this->instanceSet(REF_CLASS_DIRECT_SUBCLASSES, _Nil<T_O>());
  this->instanceSet(REF_CLASS_DIRECT_DEFAULT_INITARGS, _Nil<T_O>());
  this->instanceSet(REF_CLASS_FINALIZED, _Nil<T_O>());
  this->instanceSet(REF_CLASS_SEALEDP, _Nil<T_O>());
  this->instanceSet(REF_CLASS_DEPENDENTS, _Nil<T_O>());
  this->instanceSet(REF_CLASS_LOCATION_TABLE, _Nil<T_O>());
  this->instanceSet(REF_CLASS_INSTANCE_STAMP, clasp_make_fixnum(stamp));
  this->instanceSet(REF_CLASS_CREATOR, creator);
}


CL_DEFUN List_sp core__class_slot_sanity_check()
{
  List_sp sanity = _Nil<T_O>();
#define ADD_SANITY_CHECK(symbol_name,ref)   sanity = Cons_O::create(Cons_O::create(clos::_sym_##symbol_name, core::clasp_make_fixnum(Class_O::REF_CLASS_##ref)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_name, core::clasp_make_fixnum(Class_O::REF_CLASS_CLASS_NAME)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_DIRECT_SUPERCLASSES, core::clasp_make_fixnum(Class_O::REF_CLASS_DIRECT_SUPERCLASSES)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_SLOTS, core::clasp_make_fixnum(Class_O::REF_CLASS_SLOTS)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_DIRECT_DEFAULT_INITARGS, core::clasp_make_fixnum(Class_O::REF_CLASS_DIRECT_DEFAULT_INITARGS)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_FINALIZED, core::clasp_make_fixnum(Class_O::REF_CLASS_FINALIZED)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_PRECEDENCE_LIST, core::clasp_make_fixnum(Class_O::REF_CLASS_CLASS_PRECEDENCE_LIST)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_DIRECT_SLOTS, core::clasp_make_fixnum(Class_O::REF_CLASS_DIRECT_SLOTS)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_DEFAULT_INITARGS, core::clasp_make_fixnum(Class_O::REF_CLASS_DEFAULT_INITARGS)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_NUMBER_OF_SLOTS_IN_STANDARD_CLASS, core::clasp_make_fixnum(REF_CLASS_NUMBER_OF_SLOTS_IN_STANDARD_CLASS)),sanity);
  sanity = Cons_O::create(Cons_O::create(clos::_sym_NUMBER_OF_SLOTS_IN_STRUCTURE_CLASS, core::clasp_make_fixnum(REF_CLASS_NUMBER_OF_SLOTS_IN_STRUCTURE_CLASS)),sanity);
#define ADD_SANITY_CHECK_SIMPLE(symbol_name)   sanity = Cons_O::create(Cons_O::create(clos::_sym_##symbol_name, core::clasp_make_fixnum(Class_O::REF_CLASS_##symbol_name)),sanity);
  ADD_SANITY_CHECK_SIMPLE(DIRECT_SUBCLASSES);
  ADD_SANITY_CHECK_SIMPLE(SEALEDP);
  ADD_SANITY_CHECK_SIMPLE(DEPENDENTS);
  ADD_SANITY_CHECK_SIMPLE(LOCATION_TABLE);
  return sanity;
}

T_sp Instance_O::oinstancep() const {
  return make_fixnum((gctools::Fixnum)(this->numberOfSlots()));
}

T_sp Instance_O::oinstancepSTAR() const {
    return make_fixnum((gctools::Fixnum)(this->numberOfSlots()));
  }

/*! See ECL>>instance.d>>si_allocate_instance */
Instance_sp allocate_instance(Class_sp cl, size_t numberOfSlots) {
  ASSERT(cl->CLASS_has_creator());
  Creator_sp creator = gctools::As<Creator_sp>(cl->CLASS_get_creator());
  Instance_sp obj = gctools::As<Instance_sp>(creator->creator_allocate());
  obj->_Class = cl;
  obj->initializeSlots(cl->_get_instance_stamp(),numberOfSlots);
  return obj;
}

/*! See ECL>>instance.d>>si_allocate_raw_instance */
CL_LISPIFY_NAME(allocate_raw_instance);
CL_DEFUN T_sp core__allocate_raw_instance(T_sp orig, Class_sp class_, size_t numberOfSlots) {
  if (class_->CLASS_get_creator()->creates_classes()) {
    return core__allocate_raw_class(orig,class_,numberOfSlots);
  }
  Instance_sp output = allocate_instance(class_, numberOfSlots);
  if (orig.nilp()) return output;
  ASSERT(gc::IsA<Instance_sp>(orig));
  Instance_sp iorig = gc::As_unsafe<Instance_sp>(orig);
  iorig->_Class = class_;
  iorig->_Rack = output->_Rack; // orig->adoptSlots(output);
  return iorig;
}

T_sp Instance_O::allocate_class(Class_sp metaClass, int slots) {
  Instance_sp newClass = this->CLASS_get_creator()->creator_allocate();
  newClass->initializeSlots(metaClass->_get_instance_stamp(),slots);
  newClass->_Class = metaClass;
  return newClass;
}


CL_LAMBDA(original meta-class slots &optional creates-classes);
CL_DECLARE();
CL_DOCSTRING(R"doc(allocate-raw-class - behaves like ECL instance::allocate_raw_instance)doc");
CL_DEFUN T_sp core__allocate_raw_class(T_sp orig, Class_sp cMetaClass, int slots, bool creates_classes) {
  Instance_sp newClass = cMetaClass->allocate_class(cMetaClass,slots);
  Creator_sp cb = _Unbound<Creator_O>();
  if (creates_classes) {
    cb = gctools::GC<core::BuiltInObjectCreator<Class_O>>::allocate();
  };
  newClass->initializeClassSlots(cb,gctools::NextStamp());
  if (orig.nilp()) return newClass;
  ASSERT(gc::IsA<Class_sp>(orig));
  Instance_sp iorig = gc::As_unsafe<Instance_sp>(orig);
  iorig->_Class = cMetaClass;
  iorig->_Rack = newClass->_Rack;
  return iorig;
};





size_t Instance_O::rack_stamp_offset() {
  SimpleVector_O dummy_rack(0);
  return (char*)&(dummy_rack.operator[](0))-(char*)&dummy_rack;
}

Fixnum Instance_O::stamp() const {
  return (*this->_Rack)[0].unsafe_fixnum();
};

void Instance_O::stamp_set(Fixnum s) {
  (*this->_Rack)[0] = clasp_make_fixnum(s);
};

size_t Instance_O::numberOfSlots() const {
  return this->_Rack->length()-1;
};


T_sp Instance_O::instanceSigSet() {
  T_sp classSlots(_Nil<T_O>());
  Class_sp mc = this->_instanceClass();
  classSlots = mc->slots();
  this->_Sig = classSlots;
  return ((classSlots));
}

T_sp Instance_O::instanceSig() const {
#if DEBUG_CLOS >= 2
  stringstream ssig;
  if (this->_Sig) {
    ssig << this->_Sig->__repr__();
  } else {
    ssig << "UNDEFINED ";
  }
  printf("\nMLOG INSTANCE-SIG of Instance %p \n", (void *)(this));
#endif
  return ((this->_Sig));
}



SYMBOL_EXPORT_SC_(ClosPkg, setFuncallableInstanceFunction);
SYMBOL_EXPORT_SC_(CorePkg, instanceClassSet);



T_sp Instance_O::instanceClassSet(Class_sp mc) {
  this->_Class = mc;
  return (this->sharedThis<Instance_O>());
#ifdef DEBUG_GUARD_VALIDATE
  client_validate(this->_Rack);
#endif
}

T_sp Instance_O::instanceRef(size_t idx) const {
#ifdef DEBUG_GUARD_VALIDATE
  client_validate(this->_Rack);
#endif
#if DEBUG_CLOS >= 2
  printf("\nMLOG INSTANCE-REF[%d] of Instance %p --->%s\n", idx, (void *)(this), this->_Rack[idx+1]->__repr__().c_str());
#endif
  return ((*this->_Rack)[idx+1]);
}
T_sp Instance_O::instanceSet(size_t idx, T_sp val) {
#if DEBUG_CLOS >= 2
  printf("\nMLOG SI-INSTANCE-SET[%d] of Instance %p to val: %s\n", idx, (void *)(this), val->__repr__().c_str());
#endif
  (*this->_Rack)[idx+1] = val;
#ifdef DEBUG_GUARD_VALIDATE
  client_validate(this->_Rack);
#endif
  return val;
}

string Instance_O::__repr__() const {
  stringstream ss;
  ss << "#S(";
  if (Class_sp mc = this->_Class.asOrNull<Class_O>()) {
    ss << mc->_classNameAsString() << " ";
  } else {
    ss << "<ADD SUPPORT FOR INSTANCE _CLASS=" << _rep_(this->_Class) << " >";
  }
  if (this->isgf()) {
      ss << _rep_(this->GFUN_NAME());
  }
  {
    ss << " #slots[" << this->numberOfSlots() << "]";
#if 0
    for (size_t i(1); i < this->numberOfSlots(); ++i) {
      T_sp obj = this->_Rack[i];
      ss << "        :slot" << i << " ";
      if (obj) {
        stringstream sslot;
        if ((obj).consp()) {
          sslot << "CONS...";
          ss << sslot.str() << std::endl;
        } else if (Instance_sp inst = obj.asOrNull<Instance_O>()) {
          (void)inst; // turn off warning
          sslot << "INSTANCE...";
          ss << sslot.str() << std::endl;
        } else {
          sslot << _rep_(obj);
          if (sslot.str().size() > 80) {
            ss << sslot.str().substr(0, 80) << "...";
          } else {
            ss << sslot.str();
          }
          ss << " " << std::endl;
        }
      } else {
        ss << "UNDEFINED " << std::endl;
      }
    }
#endif
  }
  ss << ")" ;
  return ss.str();
}

T_sp Instance_O::copyInstance() const {
  Instance_sp iobj = gc::As<Instance_sp>(allocate_instance(this->_Class,1));
  iobj->_isgf = this->_isgf;
  iobj->_Rack = this->_Rack;
  iobj->_entryPoint = this->_entryPoint;
  iobj->_Sig = this->_Sig;
  return iobj;
}

void Instance_O::reshapeInstance(int delta) {
  size_t copySize = this->_Rack->length();
  if (delta<0) copySize += delta;
  SimpleVector_sp newRack = SimpleVector_O::make(this->_Rack->length()+delta,_Unbound<T_O>(),true,copySize,&(*this->_Rack)[0]);
  this->_Rack = newRack;
}
/*
  memcpy(aux->instance.slots, x->instance.slots,
  (delta < 0 ? aux->instance.length : x->instance.length) *
  sizeof(cl_object));
  x->instance = aux->instance;
*/

SYMBOL_SC_(ClosPkg, standardOptimizedReaderMethod);
SYMBOL_SC_(ClosPkg, standardOptimizedWriterMethod);

void Instance_O::ensureClosure(DispatchFunction_fptr_type entryPoint) {
  this->_entryPoint = entryPoint;
};

T_sp Instance_O::setFuncallableInstanceFunction(T_sp functionOrT) {
  if (this->_isgf == CLASP_USER_DISPATCH) {
    this->reshapeInstance(-1);
    this->_isgf = CLASP_NOT_FUNCALLABLE;
  }
  SYMBOL_EXPORT_SC_(ClPkg, standardGenericFunction);
  SYMBOL_SC_(ClosPkg, standardOptimizedReaderFunction);
  SYMBOL_SC_(ClosPkg, standardOptimizedWriterFunction);
  SYMBOL_SC_(ClosPkg, invalidated_dispatch_function );
  if (functionOrT == _lisp->_true()) {
    this->_isgf = CLASP_STANDARD_DISPATCH;
    Instance_O::ensureClosure(&generic_function_dispatch);
  } else if (functionOrT == cl::_sym_standardGenericFunction) {
    this->_isgf = CLASP_RESTRICTED_DISPATCH;
    Instance_O::ensureClosure(&generic_function_dispatch);
  } else if (functionOrT == clos::_sym_invalidated_dispatch_function) {
    this->_isgf = CLASP_INVALIDATED_DISPATCH;
    Instance_O::ensureClosure(&invalidated_dispatch);
  } else if (functionOrT.nilp()) {
    this->_isgf = CLASP_NOT_FUNCALLABLE;
    Instance_O::ensureClosure(&not_funcallable_dispatch);
  } else if (functionOrT == clos::_sym_standardOptimizedReaderMethod) {
    /* WARNING: We assume that f(a,...) behaves as f(a,b) */
    this->_isgf = CLASP_READER_DISPATCH;
    // TODO: Switch to using slotReaderDispatch like ECL for improved performace
    //	    this->_Entry = &slotReaderDispatch;
    //Instance_O::ensureClosure(&generic_function_dispatch);
    Instance_O::ensureClosure(&optimized_slot_reader_dispatch);
  } else if (functionOrT == clos::_sym_standardOptimizedWriterMethod) {
    /* WARNING: We assume that f(a,...) behaves as f(a,b) */
    this->_isgf = CLASP_WRITER_DISPATCH;
    Instance_O::ensureClosure(&optimized_slot_writer_dispatch);
  } else if (gc::IsA<CompiledDispatchFunction_sp>(functionOrT)) {
    this->_isgf = CLASP_STRANDH_DISPATCH;
    this->GFUN_DISPATCHER_set(functionOrT);
    Instance_O::ensureClosure(gc::As_unsafe<CompiledDispatchFunction_sp>(functionOrT)->entryPoint());
  } else if (!cl__functionp(functionOrT)) {
    TYPE_ERROR(functionOrT, cl::_sym_function);
    //SIMPLE_ERROR(BF("Wrong type argument: %s") % functionOrT->__repr__());
  } else {
    this->reshapeInstance(+1);
    (*this->_Rack)[this->_Rack->length() - 1] = functionOrT;
    this->_isgf = CLASP_USER_DISPATCH;
    Instance_O::ensureClosure(&user_function_dispatch);
  }
  return ((this->sharedThis<Instance_O>()));
}

T_sp Instance_O::userFuncallableInstanceFunction() const
{
  if (this->_isgf == CLASP_USER_DISPATCH) {
    T_sp user_dispatch_fn = (*this->_Rack)[this->_Rack->length()-1];
    return user_dispatch_fn;
  }
  // Otherwise return NIL
  return _Nil<T_O>();
}

bool Instance_O::genericFunctionP() const {
  return (this->_isgf);
}

bool Instance_O::equalp(T_sp obj) const {
  if (!obj.generalp()) return false;
  if (this == obj.unsafe_general()) return true;
  if (Instance_sp iobj = obj.asOrNull<Instance_O>()) {
    if (this->_Class != iobj->_Class) return false;
    if (this->stamp() != iobj->stamp()) return false;
    for (size_t i(1), iEnd(this->_Rack->length()); i < iEnd; ++i) {
      if (!cl__equalp((*this->_Rack)[i], (*iobj->_Rack)[i])) return false;
    }
    return true;
  }
  return false;
}

void Instance_O::sxhash_(HashGenerator &hg) const {
  if (hg.isFilling())
    hg.hashObject(this->_Class);
  for (size_t i(1), iEnd(this->_Rack->length()); i < iEnd; ++i) {
    if (!(*this->_Rack)[i].unboundp() && hg.isFilling())
      hg.hashObject((*this->_Rack)[i]);
    else
      break;
  }
}

void Instance_O::LISP_INVOKE() {
  IMPLEMENT_ME();
#if 0
  ASSERT(this->_Entry!=NULL);
  LispCompiledFunctionIHF _frame(my_thread->invocationHistoryStack(),this->asSmartPtr());
  return(( (this->_Entry)(*this,nargs,args)));
#endif
}

LCC_RETURN Instance_O::LISP_CALLING_CONVENTION() {
  SETUP_CLOSURE(Instance_O,closure);
  INCREMENT_FUNCTION_CALL_COUNTER(closure);
  INITIALIZE_VA_LIST();
  return (closure->_entryPoint)(closure->asSmartPtr().tagged_(), lcc_vargs.tagged_());
}

void Instance_O::describe(T_sp stream) {
  stringstream ss;
  ss << (BF("Instance\n")).str();
  ss << (BF("isgf %d\n") % this->_isgf).str();
  ss << (BF("_Class: %s\n") % _rep_(this->_Class).c_str()).str();
  for (int i(1); i < this->_Rack->length(); ++i) {
    ss << (BF("_Rack[%d]: %s\n") % i % _rep_((*this->_Rack)[i]).c_str()).str();
  }
  clasp_write_string(ss.str(), stream);
}


CL_DEFUN bool core__call_history_entry_key_contains_specializer(SimpleVector_sp key, T_sp specializer) {
  if (specializer.consp()) {
    Cons_sp eql_spec(gc::As_unsafe<Cons_sp>(specializer));
    // Check and remove eql specializer
    for ( size_t i(0); i<key->length(); ++i ) {
      if (!(*key)[i].consp()) continue;
      if (cl__eql((*key)[i],oCadr(eql_spec))) return true;
    }
  } else {
    // Check and remove class specializer
    for ( size_t i(0); i<key->length(); ++i ) {
      if ((*key)[i].consp()) continue;
      if ((*key)[i] == specializer) return true;
    }
  }
  return false;
}
  

CL_DEFUN bool core__specializer_key_match(SimpleVector_sp x, SimpleVector_sp entry_key) {
  if (x->length() != entry_key->length()) return false;
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   specializer_key_match    key: %s\n", __FILE__, __LINE__, _rep_(x).c_str());
    printf("%s:%d                      entry_key: %s\n", __FILE__, __LINE__, _rep_(entry_key).c_str());
  }
#endif
  for ( size_t i(0); i<x->length(); ++i ) {
    // If eql specializer then match the specializer value
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   arg index %lu\n", __FILE__, __LINE__, i);
  }
#endif
    if ((*x)[i].consp()) {
      if (!(*entry_key)[i].consp()) goto NOMATCH;
      T_sp eql_spec_x = oCar((*x)[i]);
      T_sp eql_spec_y = oCar((*entry_key)[i]);
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   eql_spec_x -> %s  eql_spec_y -> %s\n", __FILE__, __LINE__, _rep_(eql_spec_x).c_str(),_rep_(eql_spec_y).c_str());
  }
#endif
      if (!cl__eql(eql_spec_x,eql_spec_y)) goto NOMATCH; //gc::As_unsafe<Cons_sp>(eql_spec_y)->memberEql(eql_spec_x)) goto NOMATCH;
    } else {
      if ((*x)[i] != (*entry_key)[i]) goto NOMATCH;
    }
  }
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d       MATCHED!!!!\n",__FILE__,__LINE__);
  }
#endif
  return true;
 NOMATCH:
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d       no match\n",__FILE__,__LINE__);
  }
#endif
  return false;
}




CL_DEFUN List_sp core__call_history_find_key(List_sp generic_function_call_history, SimpleVector_sp key) {
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   call_history_find_key    key: %s\n", __FILE__, __LINE__, _rep_(key).c_str());
  }
#endif
  for ( auto cur : generic_function_call_history ) {
    ASSERT(oCar(cur).consp());
    Cons_sp entry = gc::As_unsafe<Cons_sp>(oCar(cur));
    ASSERT(gc::IsA<SimpleVector_sp>(oCar(entry)));
    SimpleVector_sp entry_key = gc::As_unsafe<SimpleVector_sp>(oCar(entry));
    if (core__specializer_key_match(key,entry_key)) return cur;
  }
  return _Nil<T_O>();
}
           
    
    
/*! Return true if an entry was pushed */
CL_DEFUN bool core__generic_function_call_history_push_new(Instance_sp generic_function, SimpleVector_sp key, T_sp effective_method )
{
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   generic_function_call_history_push_new    gf: %s\n        key: %s\n          em: %s\n", __FILE__, __LINE__, _rep_(generic_function).c_str(), _rep_(key).c_str(), _rep_(effective_method).c_str());
  }
#endif
  List_sp call_history(generic_function->GFUN_CALL_HISTORY());
  if (call_history.nilp()) {
    generic_function->GFUN_CALL_HISTORY_set(Cons_O::createList(Cons_O::create(key,effective_method)));
    return true;
  }
  // Search for existing entry
  List_sp found = core__call_history_find_key(call_history,key);
  if (found.nilp()) {
    generic_function->GFUN_CALL_HISTORY_set(Cons_O::create(Cons_O::create(key,effective_method),generic_function->GFUN_CALL_HISTORY()));
    return true;
  }
  return false;
}


CL_DEFUN void core__generic_function_call_history_remove_entries_with_specializers(Instance_sp generic_function, List_sp specializers ) {
//  printf("%s:%d Remember to remove entries with subclasses of specializer: %s\n", __FILE__, __LINE__, _rep_(specializer).c_str());
#ifdef DEBUG_GFDISPATCH
  if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
    printf("%s:%d   generic-function_call_history_remove_entries_with_specializers   gf: %s\n        specializers: %s\n", __FILE__, __LINE__, _rep_(generic_function).c_str(), _rep_(specializers).c_str());
  }
#endif
  List_sp call_history(generic_function->GFUN_CALL_HISTORY());
  if (call_history.notnilp()) {
    for ( auto cur_specializer : specializers ) {
      List_sp edited(_Nil<T_O>());
      T_sp one_specializer = oCar(cur_specializer);
      for ( List_sp cur = call_history; cur.consp(); ) {
        ASSERT(oCar(cur).consp());
        Cons_sp entry = gc::As_unsafe<Cons_sp>(oCar(cur));
        ASSERT(gc::IsA<SimpleVector_sp>(oCar(entry)));
        SimpleVector_sp entry_key = gc::As_unsafe<SimpleVector_sp>(oCar(entry));
#ifdef DEBUG_GFDISPATCH
        if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
          printf("%s:%d         check if entry_key: %s   contains specializer: %s\n", __FILE__, __LINE__, _rep_(entry_key).c_str(), _rep_(one_specializer).c_str());
        }
#endif
        if (core__call_history_entry_key_contains_specializer(entry_key,one_specializer)) {
#ifdef DEBUG_GFDISPATCH
        if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
          printf("%s:%d       IT DOES!!!\n", __FILE__, __LINE__ );
        }
#endif
          cur = oCdr(cur);
        } else {
#ifdef DEBUG_GFDISPATCH
        if (_sym_STARdebug_dispatchSTAR->symbolValue().notnilp()) {
          printf("%s:%d       it does not - keeping entry!!!\n", __FILE__, __LINE__ );
        }
#endif
          Cons_sp save = gc::As_unsafe<Cons_sp>(cur);
          cur = oCdr(cur);
          save->rplacd(edited);
          edited = save;
        }
      }
      call_history = edited;
    }
    generic_function->GFUN_CALL_HISTORY_set(call_history);
  }
}
 



};





namespace core {
Class_sp Instance_O::create(Symbol_sp symbol, Class_sp metaClass, Creator_sp creator ) {
  DEPRECATED();
};

Class_sp Instance_O::createClassUncollectable(gctools::Stamp stamp, Class_sp metaClass, size_t number_of_slots, Creator_sp creator ) {
  GC_ALLOCATE_UNCOLLECTABLE(Instance_O, oclass, metaClass, number_of_slots);
  oclass->_Class = metaClass;
  oclass->initializeSlots(stamp,number_of_slots);
  oclass->initializeClassSlots(creator,gctools::NextStamp());
  return oclass;
};

void Instance_O::CLASS_set_creator(Creator_sp cb) {
#ifdef DEBUG_CLASS_INSTANCE
  printf("%s:%d    setCreator for %s @%p -> @%p\n", __FILE__, __LINE__, _rep_(this->name()).c_str(), this, cb.raw_());
#endif
  this->instanceSet(REF_CLASS_CREATOR,cb);
}

void Instance_O::accumulateSuperClasses(HashTableEq_sp supers, VectorObjects_sp arrayedSupers, Class_sp mc) {
  if (IS_SYMBOL_UNDEFINED(mc->_className()))
    return;
  //	printf("%s:%d accumulateSuperClasses of: %s\n", __FILE__, __LINE__, _rep_(mc->className()).c_str() );
  if (supers->contains(mc))
    return;
  Fixnum arraySuperLength = arrayedSupers->length();
  supers->setf_gethash(mc, clasp_make_fixnum(arraySuperLength));
  arrayedSupers->vectorPushExtend(mc);
  List_sp directSuperclasses = mc->directSuperclasses();
  //	printf("%s:%d accumulateSuperClasses arraySuperLength = %d\n", __FILE__, __LINE__, arraySuperLength->get());
  for (auto cur : directSuperclasses) // ; cur.notnilp(); cur=cCdr(cur) )
  {
    T_sp one = oCar(cur);
    Class_sp oneClass = gc::As<Class_sp>(one);
    accumulateSuperClasses(supers, arrayedSupers, oneClass);
  }
}

void Instance_O::lowLevel_calculateClassPrecedenceList() {
  using namespace boost;
  HashTableEq_sp supers = HashTableEq_O::create_default();
  VectorObjects_sp arrayedSupers(VectorObjects_O::make(16, _Nil<T_O>(), clasp_make_fixnum(0)));
  this->accumulateSuperClasses(supers, arrayedSupers, this->sharedThis<Class_O>());
  vector<list<int>> graph(cl__length(arrayedSupers));

  class TopoSortSetup : public KeyValueMapper {
  private:
    HashTable_sp supers;
    vector<list<int>> *graphP;

  public:
    TopoSortSetup(HashTable_sp asupers, vector<list<int>> *gP) : supers(asupers), graphP(gP){};
    virtual bool mapKeyValue(T_sp key, T_sp value) {
      Fixnum_sp fnValue(gc::As<Fixnum_sp>(value));
      int mcIndex = unbox_fixnum(fnValue);
      Class_sp mc = gc::As<Class_sp>(key);
      for (auto mit : (List_sp)(mc->directSuperclasses())) {
        T_sp val = this->supers->gethash(oCar(mit));
        ASSERT(val.notnilp());
        Fixnum_sp fnval = gc::As<Fixnum_sp>(val);
        int aSuperIndex = unbox_fixnum(fnval);
        (*this->graphP)[mcIndex].push_front(aSuperIndex);
      }
      return true;
    }
  };
  TopoSortSetup topoSortSetup(supers, &graph);
  supers->lowLevelMapHash(&topoSortSetup);
#ifdef DEBUG_ON
  {
    for (size_t zi(0), ziEnd(cl__length(arrayedSupers)); zi < ziEnd; ++zi) {
      stringstream ss;
      ss << (BF("graph[%d/name=%s] = ") % zi % arrayedSupers->operator[](zi).as<Class_O>()->instanceClassName()).str();
      for (list<int>::const_iterator it = graph[zi].begin(); it != graph[zi].end(); it++) {
        ss << *it << "-> ";
      }
      ss << ";";
      LOG(BF("%s") % ss.str());
    }
  }
#endif
  deque<int> topo_order;
  topological_sort(graph, front_inserter(topo_order), vertex_index_map(identity_property_map()));
#ifdef DEBUG_ON
  {
    stringstream ss;
    ss << "Topologically sorted superclasses ";
    for (deque<int>::const_reverse_iterator it = topo_order.rbegin(); it != topo_order.rend(); it++) {
      Class_sp mc = arrayedSupers->operator[](*it).as<Class_O>();
      ss << "-> " << mc->className() << "/" << mc->instanceClassName();
    }
    LOG(BF("%s") % ss.str());
  }
#endif
  List_sp cpl = _Nil<T_O>();
  for (deque<int>::const_reverse_iterator it = topo_order.rbegin(); it != topo_order.rend(); it++) {
    Class_sp mc = gc::As<Class_sp>(arrayedSupers->operator[](*it));
    LOG(BF("pushing superclass[%s] to front of ClassPrecedenceList") % mc->instanceClassName());
    cpl = Cons_O::create(mc, cpl);
  }
  this->instanceSet(REF_CLASS_CLASS_PRECEDENCE_LIST, cpl);
}

void Instance_O::addInstanceBaseClass(Symbol_sp className) {
  this->addInstanceBaseClassDoNotCalculateClassPrecedenceList(className);
  this->lowLevel_calculateClassPrecedenceList();
}

void Instance_O::setInstanceBaseClasses(List_sp classes) {
  this->instanceSet(REF_CLASS_DIRECT_SUPERCLASSES, cl__copy_list(classes));
  this->lowLevel_calculateClassPrecedenceList();
}


bool Instance_O::isSubClassOf(Class_sp ancestor) const {
#if 0
  printf("%s:%d   Checking if this[%s] isSubClassOf[%s]\n", __FILE__, __LINE__, _rep_(this->asSmartPtr()).c_str(), _rep_(ancestor).c_str());
  Class_sp find_theClass = cl__find_class(cl::_sym_class,true,_Nil<T_O>());
  if (_lisp->_Roots._TheClass != find_theClass) {
    printf("%s:%d   Instance_O::isSubClassOf  find_theClass(%p) and _lisp->_Root._TheClass(%p) don't match anymore\n", __FILE__, __LINE__, find_theClass.raw_(), _lisp->_Roots._TheClass.raw_() );
  }
#endif
  if (this->_Class==_lisp->_Roots._TheClass
      || this->_Class==_lisp->_Roots._TheBuiltInClass
      || this->_Class==_lisp->_Roots._TheStandardClass
      || this->_Class==_lisp->_Roots._TheStructureClass
      || this->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    if (this == &*ancestor) return true;
  // TODO: I need to memoize this somehow so that I'm not constantly searching a list in
  // linear time
    List_sp cpl = this->instanceRef(Class_O::REF_CLASS_CLASS_PRECEDENCE_LIST);
    ASSERTF(!cpl.unboundp(), BF("You tried to use isSubClassOf when the ClassPrecedenceList had not been initialized"));
    for (auto cur : cpl) {
      if (CONS_CAR(cur) == ancestor) return true;
    }
    return false;
  }
  printf("%s:%d FAILED   this->_Class->isSubClassOf(_lisp->_Roots._TheClass) ->%d\n", __FILE__, __LINE__, this->_Class->isSubClassOf(_lisp->_Roots._TheClass));
  {
    printf("%s:%d   this->className() -> %s  checking if subclass of %s\n", __FILE__, __LINE__, _rep_(this->_className()).c_str(), _rep_(ancestor->_className()).c_str());
    List_sp cpl = this->instanceRef(Class_O::REF_CLASS_CLASS_PRECEDENCE_LIST);
    for ( auto cur: cpl ) {
      printf("%s:%d  cpl -> %s\n", __FILE__, __LINE__, _rep_(gc::As<Instance_sp>(CONS_CAR(cur))->_className()).c_str());
    }
  }
  {
    printf("%s:%d   this->_Class->className() -> %s  its cpl ->>>\n", __FILE__, __LINE__, _rep_(this->_Class->_className()).c_str());
    List_sp cpl = this->_Class->instanceRef(Class_O::REF_CLASS_CLASS_PRECEDENCE_LIST);
    for ( auto cur: cpl ) {
      Class_sp cc = gc::As<Instance_sp>(CONS_CAR(cur));
      printf("%s:%d  cpl -> %s == _lisp->_Roots._TheClass -> %d\n", __FILE__, __LINE__, _rep_(cc->_className()).c_str(), cc == _lisp->_Roots._TheClass);
    }
  }
  TYPE_ERROR(this->asSmartPtr(),cl::_sym_class);
}

gc::Nilable<Class_sp> identifyCxxDerivableAncestorClass(Class_sp aClass) {
  if (aClass->cxxClassP()) {
    if (aClass->cxxDerivableClassP()) {
      return aClass;
    }
  }
  for (auto supers : aClass->directSuperclasses()) {
    Class_sp aSuperClass = gc::As<Class_sp>(oCar(supers));
    gc::Nilable<Class_sp> taPossibleCxxDerivableAncestorClass = identifyCxxDerivableAncestorClass(aSuperClass);
    if (taPossibleCxxDerivableAncestorClass.notnilp())
      return taPossibleCxxDerivableAncestorClass;
  }
  return _Nil<Class_O>();
}

void Instance_O::inheritDefaultAllocator(List_sp superclasses) {
  // If this class already has an allocator then leave it alone
  if (this->CLASS_has_creator()) return;
  Class_sp aCxxDerivableAncestorClass_unsafe; // Danger!  Unitialized!
#ifdef DEBUG_CLASS_INSTANCE
  printf("%s:%d:%s   for class -> %s   superclasses -> %s\n", __FILE__, __LINE__, __FUNCTION__, _rep_(this->name()).c_str(), _rep_(superclasses).c_str());
#endif
  bool derives_from_StandardClass = false;
  for (auto cur : superclasses) {
    T_sp tsuper = oCar(cur);
    if (tsuper == _lisp->_Roots._TheStandardClass) {
      derives_from_StandardClass = true;
#ifdef DEBUG_CLASS_INSTANCE
      printf("%s:%d:%s        derives from class\n", __FILE__, __LINE__, __FUNCTION__ );
#endif
    }
    if (Class_sp aSuperClass = tsuper.asOrNull<Class_O>() ) {
      if (aSuperClass->cxxClassP() && !aSuperClass->cxxDerivableClassP()) {
        SIMPLE_ERROR(BF("You cannot derive from the non-derivable C++ class %s\n"
                        "any C++ class you want to derive from must inherit from the clbind derivable class") %
                     _rep_(aSuperClass->_className()));
      }
      gc::Nilable<Class_sp> aPossibleCxxDerivableAncestorClass = identifyCxxDerivableAncestorClass(aSuperClass);
      if (aPossibleCxxDerivableAncestorClass.notnilp()) {
        if (!aCxxDerivableAncestorClass_unsafe) {
          aCxxDerivableAncestorClass_unsafe = aPossibleCxxDerivableAncestorClass;
        } else {
          SIMPLE_ERROR(BF("Only one derivable C++ class is allowed to be"
                          " derived from at a time instead we have two %s and %s ") %
                       _rep_(aCxxDerivableAncestorClass_unsafe->_className()) % _rep_(aPossibleCxxDerivableAncestorClass->_className()));
        }
      }
    } else if ( Instance_sp iSuperClass = tsuper.asOrNull<Instance_O>() ) {
      SIMPLE_ERROR(BF("In Clasp, Instances are never Classes - so this error should never occur.  If it does - figure out why tsuper is an Instance"));
      // I don't think I do anything here
      // If aCxxDerivableAncestorClass_unsafe is left unchanged then
      // an InstanceCreator_O will be created for this class.
    }
  }
  if (aCxxDerivableAncestorClass_unsafe) {
    // Here aCxxDerivableAncestorClass_unsafe has a value - so it's ok to dereference it
    Creator_sp aCxxAllocator(gctools::As<Creator_sp>(aCxxDerivableAncestorClass_unsafe->CLASS_get_creator()));
#ifdef DEBUG_CLASS_INSTANCE
    printf("%s:%d   duplicating aCxxDerivableAncestorClass_unsafe %s creator\n", __FILE__, __LINE__, _rep_(aCxxDerivableAncestorClass_unsafe).c_str());
#endif
    Creator_sp dup = aCxxAllocator->duplicateForClassName(this->_className());
    this->CLASS_set_creator(dup); // this->setCreator(dup.get());
  } else if (derives_from_StandardClass) {
#ifdef DEBUG_CLASS_INSTANCE
    printf("%s:%d   Creating a ClassCreator for %s\n", __FILE__, __LINE__, _rep_(this->name()).c_str());
#endif
    ClassCreator_sp classCreator = gc::GC<ClassCreator_O>::allocate(this->asSmartPtr());
    this->CLASS_set_creator(classCreator);
  } else {
    // I think this is the most common outcome -
#ifdef DEBUG_CLASS_INSTANCE
    printf("%s:%d   Creating an InstanceCreator_O for the class: %s\n", __FILE__, __LINE__, _rep_(this->name()).c_str());
#endif
    InstanceCreator_sp instanceAllocator = gc::GC<InstanceCreator_O>::allocate(this->asSmartPtr());
    //gctools::StackRootedPointer<InstanceCreator> instanceAllocator(new InstanceCreator(this->name()));
    this->CLASS_set_creator(instanceAllocator); // this->setCreator(instanceAllocator.get());
  }
}

void Instance_O::addInstanceBaseClassDoNotCalculateClassPrecedenceList(Symbol_sp className) {
  _OF();
  Class_sp cl = gc::As<Class_sp>(eval::funcall(cl::_sym_findClass, className, _lisp->_true()));
  // When booting _DirectSuperClasses may be undefined
  List_sp dsc = this->directSuperclasses();
  this->instanceSet(REF_CLASS_DIRECT_SUPERCLASSES, Cons_O::create(cl, dsc));
}



void Instance_O::__setupStage3NameAndCalculateClassPrecedenceList(Symbol_sp className) {
  this->_setClassName(className);
  // Initialize some of the class slots
  this->instanceSet(REF_CLASS_DIRECT_SLOTS,_Nil<T_O>());
  this->instanceSet(REF_CLASS_DEFAULT_INITARGS,_Nil<T_O>());
  T_sp tmc = this->_instanceClass();
  ASSERTNOTNULL(tmc);
  Class_sp mc = gc::As<Class_sp>(tmc);
  (void)mc;
  this->lowLevel_calculateClassPrecedenceList();
}


};



namespace core {

string Instance_O::dumpInfo() {
  stringstream ss;
  ss << (boost::format("this.instanceClassName: %s @ %X") % this->instanceClassName() % this) << std::endl;
  ss << "_FullName[" << this->_className()->fullName() << "]" << std::endl;
  ss << boost::format("    _Class = %X  this._Class.instanceClassName()=%s\n") % this->__class().get() % this->__class()->instanceClassName();
  for (auto cc : this->directSuperclasses()) {
    ss << "Base class: " << gc::As<Class_sp>(oCar(cc))->instanceClassName() << std::endl;
  }
  ss << boost::format("this.instanceCreator* = %p") % (void *)(&*this->CLASS_get_creator()) << std::endl;
  return ss.str();
}

string Class_O::getPackagedName() const {
  return this->_className()->formattedName(false);
}

string Instance_O::_classNameAsString() const {
  return _rep_(this->_className());
}


T_sp Instance_O::make_instance() {
  T_sp instance = this->CLASS_get_creator()->creator_allocate(); //this->allocate_newNil();
  if (instance.generalp()) {
    instance.unsafe_general()->initialize();
  } else {
    SIMPLE_ERROR(BF("Add support to make_instance of non general objects"));
  }
  return instance;
}

List_sp Instance_O::directSuperclasses() const {
  T_sp obj = this->instanceRef(REF_CLASS_DIRECT_SUPERCLASSES);
  ASSERT(obj);
  ASSERT(!obj.unboundp());
  return coerce_to_list(obj);
}
};


/* The following functions should only work for Classes */
namespace core {
CL_DEFUN List_sp clos__direct_superclasses(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    return c->directSuperclasses();
  }
  TYPE_ERROR(c,cl::_sym_class);
}

CL_DEFUN void core__reinitialize_class(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    c->instanceSet(Instance_O::REF_CLASS_INSTANCE_STAMP,clasp_make_fixnum(gctools::NextStamp()));
    return;
  }
  TYPE_ERROR(c,cl::_sym_class);
}

CL_DEFUN Fixnum core__get_instance_stamp(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    return c->_get_instance_stamp();
  }
  TYPE_ERROR(c,cl::_sym_class);
};

CL_DEFUN T_sp core__name_of_class(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    return c->_className();
  }
  TYPE_ERROR(c,cl::_sym_class);
};

CL_DEFUN T_sp core__class_creator(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    return c->CLASS_get_creator();
  }
  TYPE_ERROR(c,cl::_sym_class);
};

CL_DEFUN bool core__has_creator(Class_sp c) {
  if (c->_Class->isSubClassOf(_lisp->_Roots._TheClass)) {
    return c->CLASS_has_creator();
  }
  TYPE_ERROR(c,cl::_sym_class);
};




};
