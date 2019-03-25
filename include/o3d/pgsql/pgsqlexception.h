/**
 * @file pgsqlexception.h
 * @brief 
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2019-03-09
 * @copyright Copyright (c) 2001-2019 Dream Overflow. All rights reserved.
 * @details 
 */

#ifndef _O3D_PGSQLEXCEPTION_H
#define _O3D_PGSQLEXCEPTION_H

#include "pgsql.h"
#include <o3d/core/error.h>

namespace o3d {
namespace pgsql {

//! @class E_PgSqlError Unable to connect to MySql server
class O3D_PGSQL_API E_PgSqlError : public E_BaseException
{
    O3D_E_DEF_CLASS(E_PgSqlError)

    //! Ctor
    E_PgSqlError(const String& msg) : E_BaseException(msg)
        O3D_E_DEF(E_PgSqlError,"PgSql error")
};

} // namespace pgsql
} // namespace o3d

#endif // _O3D_PGSQLEXCEPTION_H
