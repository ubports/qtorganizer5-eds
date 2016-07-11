/*
 * Copyright 2016 Canonical Ltd.
 *
 * This file is part of qtorganizer5-eds.
 *
 * contact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSCOPEDPOINTER__
#define __GSCOPEDPOINTER__


class GScopedPointerUnref
{
public:
    static inline void cleanup(void *pointer)
    {
        if (pointer) {
            g_clear_object(&pointer);
        }
    }
};

template<class KLASS>
class GScopedPointer : public QScopedPointer<KLASS, GScopedPointerUnref>
{
public:
    GScopedPointer(KLASS* obj = 0)
        : QScopedPointer<KLASS, GScopedPointerUnref>(obj)
    {}
};

#endif
