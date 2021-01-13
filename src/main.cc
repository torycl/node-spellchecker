#include <vector>
#include "nan.h"
#include "spellchecker.h"

using Nan::ObjectWrap;
using namespace spellchecker;
using namespace v8;

namespace {

class Spellchecker : public Nan::ObjectWrap {
  SpellcheckerImplementation* impl;
  v8::Persistent<v8::Value> dictData;

  static NAN_METHOD(New) {
    Nan::HandleScope scope;
    Spellchecker* that = new Spellchecker();
    that->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
  }

  static NAN_METHOD(SetDictionary) {
    Nan::HandleScope scope;

    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());

    bool has_contents = false;
    if (info.Length() > 1) {
      if (!node::Buffer::HasInstance(info[1])) {
        return Nan::ThrowError("SetDictionary 2nd argument must be a Buffer");
      }

      // NB: We must guarantee that we pin this Buffer
      that->dictData.Reset(info.GetIsolate(), info[1]);
      has_contents = true;
    }

    std::string language = *String::Utf8Value(v8::Isolate::GetCurrent(), info[0]);

    bool result;
    if (has_contents) {
      result = that->impl->SetDictionaryToContents(
        (unsigned char*)node::Buffer::Data(info[1]),
        node::Buffer::Length(info[1]));
    } else {
      result = that->impl->SetDictionary(language);
    }

    info.GetReturnValue().Set(Nan::New(result));
  }

  static NAN_METHOD(IsMisspelled) {
    Nan::HandleScope scope;
    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());
    std::string word = *String::Utf8Value(v8::Isolate::GetCurrent(), info[0]);

    info.GetReturnValue().Set(Nan::New(that->impl->IsMisspelled(word)));
  }

  static NAN_METHOD(CheckSpelling) {
    Nan::HandleScope scope;
    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Local<String> str = Local<String>::Cast(info[0]);
    if (!str->IsString()) {
      return Nan::ThrowError("Bad argument");
    }

    Local<Array> result = Nan::New<Array>();
    info.GetReturnValue().Set(result);

    if (str->Length() == 0) {
      return;
    }

    std::vector<uint16_t> text(str->Length() + 1);
    str->Write(v8::Isolate::GetCurrent(), reinterpret_cast<uint16_t *>(text.data()));

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());
    std::vector<MisspelledRange> misspelled_ranges = that->impl->CheckSpelling(text.data(), text.size());

    std::vector<MisspelledRange>::const_iterator iter = misspelled_ranges.begin();
    for (; iter != misspelled_ranges.end(); ++iter) {
      size_t index = iter - misspelled_ranges.begin();
      uint32_t start = iter->start, end = iter->end;

      Local<Object> misspelled_range = Nan::New<Object>();
      Nan::Set(misspelled_range, Nan::New("start").ToLocalChecked(), Nan::New<Integer>(start));
      Nan::Set(misspelled_range, Nan::New("end").ToLocalChecked(), Nan::New<Integer>(end));
      Nan::Set(result, index, misspelled_range);
    }
  }

  static NAN_METHOD(Add) {
    Nan::HandleScope scope;
    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());
    std::string word = *String::Utf8Value(v8::Isolate::GetCurrent() , info[0]);

    that->impl->Add(word);
    return;
  }

  static NAN_METHOD(Remove) {
    Nan::HandleScope scope;
    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());
    std::string word = *String::Utf8Value(v8::Isolate::GetCurrent(), info[0]);

    that->impl->Remove(word);
    return;
  }


  static NAN_METHOD(GetAvailableDictionaries) {
    Nan::HandleScope scope;

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());

    std::string path = ".";
    if (info.Length() > 0) {
      std::string path = *String::Utf8Value(v8::Isolate::GetCurrent(), info[0]);
    }

    std::vector<std::string> dictionaries =
      that->impl->GetAvailableDictionaries(path);

    Local<Array> result = Nan::New<Array>(dictionaries.size());
    for (size_t i = 0; i < dictionaries.size(); ++i) {
      const std::string& dict = dictionaries[i];
      Nan::Set(result, i, Nan::New(dict.data(), dict.size()).ToLocalChecked());
    }

    info.GetReturnValue().Set(result);
  }

  static NAN_METHOD(GetCorrectionsForMisspelling) {
    Nan::HandleScope scope;
    if (info.Length() < 1) {
      return Nan::ThrowError("Bad argument");
    }

    Spellchecker* that = Nan::ObjectWrap::Unwrap<Spellchecker>(info.Holder());

    std::string word = *String::Utf8Value(v8::Isolate::GetCurrent(), info[0]);
    std::vector<std::string> corrections =
      that->impl->GetCorrectionsForMisspelling(word);

    Local<Array> result = Nan::New<Array>(corrections.size());
    for (size_t i = 0; i < corrections.size(); ++i) {
      const std::string& word = corrections[i];

      Nan::MaybeLocal<String> val = Nan::New<String>(word.data(), word.size());
      Nan::Set(result, i, val.ToLocalChecked());
    }

    info.GetReturnValue().Set(result);
  }

  Spellchecker() {
    impl = SpellcheckerFactory::CreateSpellchecker();
  }

  // actual destructor
  virtual ~Spellchecker() {
    delete impl;
  }

 public:
  static void Init(Local<Object> exports) {
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(Spellchecker::New);

    tpl->SetClassName(Nan::New<String>("Spellchecker").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetMethod(tpl->InstanceTemplate(), "setDictionary", Spellchecker::SetDictionary);
    Nan::SetMethod(tpl->InstanceTemplate(), "getAvailableDictionaries", Spellchecker::GetAvailableDictionaries);
    Nan::SetMethod(tpl->InstanceTemplate(), "getCorrectionsForMisspelling", Spellchecker::GetCorrectionsForMisspelling);
    Nan::SetMethod(tpl->InstanceTemplate(), "isMisspelled", Spellchecker::IsMisspelled);
    Nan::SetMethod(tpl->InstanceTemplate(), "checkSpelling", Spellchecker::CheckSpelling);
    Nan::SetMethod(tpl->InstanceTemplate(), "add", Spellchecker::Add);
    Nan::SetMethod(tpl->InstanceTemplate(), "remove", Spellchecker::Remove);

    Nan::Set(exports, Nan::New("Spellchecker").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
  }
};

NAN_MODULE_INIT(Init) {
  Spellchecker::Init(target);
}

}  // namespace

NODE_MODULE(spellchecker, Init)
