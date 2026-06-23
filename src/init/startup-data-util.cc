// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/startup-data-util.h"

#include <stdlib.h>
#include <string.h>

#include "include/v8-initialization.h"
#include "include/v8-snapshot.h"
#include "src/base/file-utils.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"

#if defined(V8_OS_LINUX)
#include <sys/mman.h>
#endif

namespace v8 {
namespace internal {

#ifdef V8_USE_EXTERNAL_STARTUP_DATA

namespace {

v8::StartupData g_snapshot;

void ClearStartupData(v8::StartupData* data) {
  data->data = nullptr;
  data->raw_size = 0;
  data->is_file_backed = false;
}

void DeleteStartupData(v8::StartupData* data) {
#if defined(V8_OS_LINUX)
  if (data->IsFileBacked()) {
    if (data->data != nullptr) {
      munmap(const_cast<char*>(data->data),
             static_cast<size_t>(data->raw_size));
    }
    ClearStartupData(data);
    return;
  }
#endif  // defined(V8_OS_LINUX)
  delete[] data->data;
  ClearStartupData(data);
}

void FreeStartupData() {
  DeleteStartupData(&g_snapshot);
}

void Load(const char* blob_file, v8::StartupData* startup_data,
          void (*setter_fn)(v8::StartupData*)) {
  ClearStartupData(startup_data);

  CHECK(blob_file);

  FILE* file = base::Fopen(blob_file, "rb");
  if (!file) {
    PrintF(stderr, "Failed to open startup resource '%s'.\n", blob_file);
    return;
  }

  fseek(file, 0, SEEK_END);
  startup_data->raw_size = static_cast<int>(ftell(file));
  rewind(file);

#if defined(V8_OS_LINUX)
  void* mapping = mmap(nullptr, static_cast<size_t>(startup_data->raw_size),
                       PROT_READ, MAP_PRIVATE, fileno(file), 0);
  if (mapping != MAP_FAILED) {
    startup_data->data = reinterpret_cast<char*>(mapping);
    startup_data->is_file_backed = true;
    (*setter_fn)(startup_data);
    base::Fclose(file);
    return;
  }
  // Otherwise fall back to reading the bytes into a heap buffer below.
#endif

  startup_data->data = new char[startup_data->raw_size];
  int read_size = static_cast<int>(fread(const_cast<char*>(startup_data->data),
                                         1, startup_data->raw_size, file));
  base::Fclose(file);

  if (startup_data->raw_size == read_size) {
    (*setter_fn)(startup_data);
  } else {
    PrintF(stderr, "Corrupted startup resource '%s'.\n", blob_file);
  }
}

void LoadFromFile(const char* snapshot_blob) {
  Load(snapshot_blob, &g_snapshot, v8::V8::SetSnapshotDataBlob);
  atexit(&FreeStartupData);
}

}  // namespace
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

void InitializeExternalStartupData(const char* directory_path) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  const char* snapshot_name = "snapshot_blob.bin";
  std::unique_ptr<char[]> snapshot =
      base::RelativePath(directory_path, snapshot_name);
  LoadFromFile(snapshot.get());
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

void InitializeExternalStartupDataFromFile(const char* snapshot_blob) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  LoadFromFile(snapshot_blob);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

}  // namespace internal
}  // namespace v8
