/*
 * refind/screen_edit.h
 *
 * Line-editing functions borrowed from gummiboot
 *
 */
/*
 * Simple UEFI boot loader which executes configured EFI images, where the
 * default entry is selected by a configured pattern (glob) or an on-screen
 * menu.
 *
 * All gummiboot code is LGPL not GPL, to stay out of politics and to give
 * the freedom of copying code from programs to possible future libraries.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * Copyright (C) 2012-2013 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2012 Harald Hoyer <harald@redhat.com>
 *
 * "Any intelligent fool can make things bigger, more complex, and more violent.
"
 *   -- Albert Einstein
 */

#ifndef __LINE_EDIT_H_
#define __LINE_EDIT_H_

BOOLEAN line_edit(CHAR16 *line_in, CHAR16 **line_out, UINTN x_max);

#endif
