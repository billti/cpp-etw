// Copyright 2019 Bill Ticehurst. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./chakra-etw-provider.h"

#include <string>

namespace chakra {

ChakraEtwProvider::ChakraEtwProvider()
    : EtwProvider(chakra_provider_guid, chakra_provider_name) {}

ChakraEtwProvider& ChakraEtwProvider::GetProvider() {
  // The below pattern means the destructor will not run at process exit. (Which
  // is unnecessary anyway as the provider will unregister at process exit).
  // See "Static and Global Variables" in https://google.github.io/styleguide/cppguide.html
  static ChakraEtwProvider &the_provider = *(new ChakraEtwProvider());
  return the_provider;
}

}  // namespace chakra
