/*
   Copyright 2015 Technical University of Denmark, DTU Compute.
   All rights reserved.

   This file is part of the ahead-of-time bytecode compiler Fernando.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

      1. Redistributions of source code must retain the above copyright notice,
         this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   The views and conclusions contained in the software and documentation are
   those of the authors and should not be interpreted as representing official
   policies, either expressed or implied, of the copyright holder.
*/

#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <iconv.h>
#include <pthread.h>
#include "jvm.h"

#define DEFAULT_HEAP_SIZE (1024*1024)

pthread_key_t currentThread;

pthread_mutex_t globalLock = PTHREAD_MUTEX_INITIALIZER;

int32_t *allocPtr;
int32_t *allocEnd;

_java_lang_NullPointerException_obj_t npExc = { &_java_lang_NullPointerException, 0, };
_java_lang_ArrayIndexOutOfBoundsException_obj_t abExc = { &_java_lang_ArrayIndexOutOfBoundsException, 0, };
_java_lang_ClassCastException_obj_t ccExc = { &_java_lang_ClassCastException, 0, };
_java_lang_ArithmeticException_obj_t aeExc = { &_java_lang_ArithmeticException, 0, };
_java_lang_InterruptedException_obj_t intrExc = { &_java_lang_InterruptedException, 0, };
_java_lang_OutOfMemoryError_obj_t omErr = { &_java_lang_OutOfMemoryError, 0, };
_java_lang_VirtualMachineError_obj_t vmErr = { &_java_lang_VirtualMachineError, 0, };

_java_lang_Thread_obj_t mainThread = { &_java_lang_Thread, 0, };
pthread_t main_pthread;

void jvm_clinit(int32_t *exc) {

  long int heapSize = DEFAULT_HEAP_SIZE;
  char *envHeapSize = getenv("FERNANDO_HEAP_SIZE");
  if (envHeapSize) {
    char *endPtr;
    long int size = strtol(envHeapSize, &endPtr, 0);
    if (*envHeapSize != '\0' && *endPtr == '\0') {
      heapSize = size;
    }
  }

  allocPtr = malloc(heapSize);
  if (!allocPtr) {
    *exc = (int32_t)&omErr;
    return;
  }
  allocEnd = allocPtr+(heapSize >> 2);

  main_pthread = pthread_self();
  pthread_key_create(&currentThread, NULL);

  setlocale(LC_ALL, "");
}

void jvm_init(int32_t *retexc) {
  int32_t exc = 0;
  _java_lang_NullPointerException__init___V((int32_t)&npExc, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_ArrayIndexOutOfBoundsException__init___V((int32_t)&abExc, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_ClassCastException__init___V((int32_t)&ccExc, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_ArithmeticException__init___V((int32_t)&aeExc, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_InterruptedException__init___V((int32_t)&intrExc, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_OutOfMemoryError__init___V((int32_t)&omErr, &exc);
  if (exc != 0) { *retexc = exc; return; }
  _java_lang_VirtualMachineError__init___V((int32_t)&vmErr, &exc);
  if (exc != 0) { *retexc = exc; return; }

  _java_lang_Thread__init___V((int32_t)&mainThread, &exc);
  if (exc != 0) { *retexc = exc; return; }
  jvm_putfield(_java_lang_Thread_obj_t, &mainThread, 0, _pthread, (int32_t)&main_pthread);
  pthread_setspecific(currentThread, &mainThread);
}

int32_t jvm_args(int argc, char **argv, int32_t *exc) {
  // create String array to hold arguments
  int32_t args = (int32_t)jvm_alloc(&_java_lang_String__, sizeof(_java_lang_String___obj_t)+(argc-1)*4, exc);
  jvm_setarrlength(_java_lang_String___obj_t, args, argc-1);

  int i;
  for (i = 1; i < argc; i++) {
    // convert argument to character array
    int32_t len = strlen(argv[i]);
    uint16_t buf [len];
    int32_t bytes = jvm_encode(argv[i], len, buf, len*sizeof(buf[0]));
    // store converted characters in Java array
    int32_t arr = (int32_t)jvm_alloc(&_char__, sizeof(_char___obj_t)+bytes, exc);
    int32_t arrlen = bytes/sizeof(buf[0]);
    jvm_setarrlength(_char___obj_t, arr, arrlen);
    for (int k = 0; k < arrlen; k++) {
      jvm_arrstore(_char___obj_t, arr, k, buf[k]);
    }
    // create String from character array
    int32_t arg = (int32_t)jvm_alloc(&_java_lang_String, sizeof(_java_lang_String_obj_t), exc);
    _java_lang_String__init___C_V(arg, arr, exc);
    // store argument in array of arguments
    jvm_arrstore(_java_lang_String___obj_t, args, i-1, arg);
  }

  return args;
}

static int init_lock(_java_lang_Object_obj_t *obj) {
  int retval = 0;
  if (obj->lock == NULL) {
    if (pthread_mutex_lock(&globalLock)) {
      return -1;
    }
    if (obj->lock == NULL) {
      obj->lock = malloc(sizeof(pthread_mutex_t));
      if (obj->lock) {
        retval = pthread_mutex_init(obj->lock, NULL);
      } else {
        retval = -1;
      }
    }
    if (pthread_mutex_unlock(&globalLock)) {
      return -1;
    }
  }
  return retval;
}

int jvm_lock(_java_lang_Object_obj_t *ref) {
  if (init_lock(ref)) {
    return -1;
  }
  return pthread_mutex_lock(ref->lock);
}

int jvm_unlock(_java_lang_Object_obj_t *ref) {
  if (!ref->lock) {
    return -1;
  }
  return pthread_mutex_unlock(ref->lock);
}

static int init_wait(_java_lang_Object_obj_t *obj) {
  int retval = 0;
  if (obj->wait == NULL) {
    if (pthread_mutex_lock(&globalLock)) {
      return -1;
    }
    if (obj->wait == NULL) {
      obj->wait = malloc(sizeof(pthread_cond_t));
      if (obj->wait) {
        retval = pthread_cond_init(obj->wait, NULL);
      } else {
        retval = -1;
      }
    }
    if (pthread_mutex_unlock(&globalLock)) {
      return -1;
    }
  }
  return retval;
}

int jvm_wait(_java_lang_Object_obj_t *ref) {
  if (init_wait(ref)) {
    return -1;
  }
  return pthread_cond_wait(ref->wait, ref->lock);
}
int jvm_notify(_java_lang_Object_obj_t *ref) {
  if (init_wait(ref)) {
    return -1;
  }
  return pthread_cond_signal(ref->wait);
}
int jvm_notify_all(_java_lang_Object_obj_t *ref) {
  if (init_wait(ref)) {
    return -1;
  }
  return pthread_cond_broadcast(ref->wait);
}

int32_t jvm_instanceof(const _java_lang_Object_class_t *ref,
                       const _java_lang_Object_class_t *type) {
  if (ref == 0) {
    return 0;
  }
  if (ref == type) {
    return 1;
  }
  if (ref->elemtype != 0 && type->elemtype != 0) {
    return jvm_instanceof(ref->elemtype, type->elemtype);
  }
  return jvm_instanceof(ref->super, type);
}

int32_t jvm_encode(char *inbuf, int32_t inbytes, uint16_t *outbuf, int32_t outbytes) {
  iconv_t conv = iconv_open("UTF-16LE//TRANSLIT", "//");
  int32_t maxbytes = outbytes;
  int32_t len = iconv(conv, &inbuf, (size_t *)&inbytes,
                      (char **)&outbuf, (size_t *)&outbytes);
  iconv_close(conv);
  if (len < 0) {
    return len;
  } else {
    return maxbytes-outbytes;
  }
}

int32_t jvm_decode(uint16_t *inbuf, int32_t inbytes, char *outbuf, int32_t outbytes) {
  iconv_t conv = iconv_open("//TRANSLIT", "UTF-16LE//");
  int32_t maxbytes = outbytes;
  int32_t len = iconv(conv, (char **)&inbuf, (size_t *)&inbytes,
                      &outbuf, (size_t *)&outbytes);
  iconv_close(conv);
  if (len < 0) {
    return len;
  } else {
    return maxbytes-outbytes;
  }
}

void jvm_catch(int32_t exc) {
  int i;

  fflush(0);
  fprintf(stderr, "Uncaught exception: ");

  _java_lang_Throwable_obj_t *thr = (_java_lang_Throwable_obj_t *)exc;
  _java_lang_String_obj_t *name = (_java_lang_String_obj_t*)thr->type->name;
  _char___obj_t *chars = (_char___obj_t *)name->_0_value;
  int32_t chars_bytes = sizeof(chars->_1_data[0])*chars->_0_length;
  char print_buf[chars_bytes]; // this size is just a guess

  int len = jvm_decode(chars->_1_data, chars_bytes, print_buf, sizeof(print_buf));

  for (i = 0; i < len; i++) {
    fputc(print_buf[i], stderr);
  }

  fputc('\n', stderr);
  exit(EXIT_FAILURE);
}

int32_t *jvm_alloc(void *type, int32_t size, int32_t *exc) {
  if (size < 0) {
    *exc = (int32_t)&omErr;
    return allocPtr;
  }

  pthread_mutex_lock(&globalLock);
  int32_t *ptr = allocPtr;
  allocPtr += (size + 3) >> 2;
  pthread_mutex_unlock(&globalLock);

  if (allocPtr > allocEnd) {
    *exc = (int32_t)&omErr;
    return ptr;
  }
  memset(ptr, 0, (size + 3) & ~0x3);
  ((_java_lang_Object_obj_t*)ptr)->type = type;
  return ptr;
}
