/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of https://github.com/facebook/zstd.
 * An additional grant of patent rights can be found in the PATENTS file in the
 * same directory.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation. This program is dual-licensed; you may select
 * either version 2 of the GNU General Public License ("GPL") or BSD license
 * ("BSD").
 */

/* Note : this module is expected to remain private, do not expose it */

#ifndef ERROR_H_MODULE
#define ERROR_H_MODULE

/* ****************************************
*  Dependencies
******************************************/
#include "zstd.h"  /* enum list */

/*-****************************************
*  Customization (error_public.h)
******************************************/
#define PREFIX(name) ZSTD_error_##name

/*-****************************************
*  Error codes handling
******************************************/
#define ERROR(name) ((size_t)-PREFIX(name))

#define ERR_isError ZSTD_isError /* for inlining */
#define FSE_isError ZSTD_isError
#define HUF_isError ZSTD_isError
#endif /* ERROR_H_MODULE */
