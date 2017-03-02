// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BBOY_GBASE_CALLBACK_FORWARD_H_
#define BBOY_GBASE_CALLBACK_FORWARD_H_

namespace bb {

template <typename Sig>
class Callback;

typedef Callback<void(void)> Closure;

}  // namespace bb

#endif  // BBOY_GBASE_CALLBACK_FORWARD_H
