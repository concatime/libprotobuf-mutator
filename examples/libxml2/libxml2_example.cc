// Copyright 2017 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "examples/libxml2/xml_writer.h"
#include "libxml/parser.h"
#include "src/libfuzzer_protobuf_mutator.h"
#include "xml.pb.h"  // NOLINT

using examples::libxml2::Xml;

namespace {
google::protobuf::LogSilencer log_silincer;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t max_size, unsigned int seed) {
  Xml message;
  // Fuzzer can be provided with corpus of raw XML. In this case it will fail
  // to parse it as proto.
  bool is_proto = protobuf_mutator::ParseTextMessage(data, size, &message);
  // If the data is a raw XML we can store it as a content field of the proto.
  // Also we can do the same as a special kind of mutation. If the data is a
  // proto we can convert it to XML and store as a content field. This mutation
  // allows to grow coverage faster.
  if (!is_proto || seed % 33 == 0) {
    std::string xml =
        is_proto ? MessageToXml(message.document())
                 : std::string{reinterpret_cast<const char*>(data), size};
    message.mutable_document()->Clear();
    message.mutable_document()->mutable_element()->add_content()->set_char_data(
        xml);
    if (size_t new_size =
            protobuf_mutator::SaveMessageAsText(message, data, max_size)) {
      size = new_size;
    }
  }

  return protobuf_mutator::MutateTextMessage(data, size, max_size, seed,
                                             &message);
}

static void ignore(void* ctx, const char* msg, ...) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Xml message;
  auto is_proto = protobuf_mutator::ParseTextMessage(data, size, &message);
  std::string xml =
      is_proto ? MessageToXml(message.document())
               : std::string{reinterpret_cast<const char*>(data), size};
  xmlSetGenericErrorFunc(nullptr, &ignore);

  auto option = message.options();
  // Network requests are too slow.
  option |= XML_PARSE_NONET;

  // There is a known hang with this option enabled.
  // option &= ~XML_PARSE_NOENT;
  if (auto doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), "",
                               nullptr, option)) {
    xmlFreeDoc(doc);
  }

  return 0;
}