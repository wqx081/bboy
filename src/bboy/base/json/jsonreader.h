#ifndef BBOY_UTIL_JSONREADER_H_
#define BBOY_UTIL_JSONREADER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include <rapidjson/document.h>


#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/macros.h"
#include "bboy/base/status.h"

namespace bb {

// Wraps the JSON parsing functionality of rapidjson::Document.
//
// Unlike JsonWriter, this class does not hide rapidjson internals from
// clients. That's because there's just no easy way to implement object and
// array parsing otherwise. At most, this class aspires to be a simpler
// error-handling wrapper for reading and parsing.
class JsonReader {
 public:
  explicit JsonReader(std::string text);
  ~JsonReader();

  Status Init();

  // Extractor methods.
  //
  // If 'field' is not NULL, will look for a field with that name in the
  // given object, returning Status::NotFound if it cannot be found. If
  // 'field' is NULL, will try to convert 'object' directly into the
  // desire type.

  Status ExtractInt32(const rapidjson::Value* object,
                      const char* field,
                      int32_t* result) const;

  Status ExtractInt64(const rapidjson::Value* object,
                      const char* field,
                      int64_t* result) const;

  Status ExtractString(const rapidjson::Value* object,
                       const char* field,
                       std::string* result) const;

  // 'result' is only valid for as long as JsonReader is alive.
  Status ExtractObject(const rapidjson::Value* object,
                       const char* field,
                       const rapidjson::Value** result) const;

  // 'result' is only valid for as long as JsonReader is alive.
  Status ExtractObjectArray(const rapidjson::Value* object,
                            const char* field,
                            std::vector<const rapidjson::Value*>* result) const;

  const rapidjson::Value* root() const { return &document_; }

 private:
  Status ExtractField(const rapidjson::Value* object,
                      const char* field,
                      const rapidjson::Value** result) const;

  std::string text_;
  rapidjson::Document document_;

  DISALLOW_COPY_AND_ASSIGN(JsonReader);
};

} // namespace kudu

#endif // BBOY_UTIL_JSONREADER_H_
