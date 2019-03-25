/**
 * @file mysqldbvariable.h
 * @brief 
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2013-12-03
 * @copyright Copyright (c) 2001-2017 Dream Overflow. All rights reserved.
 * @details 
 */

#ifndef _O3D_PGSQLDBVARIABLE_H
#define _O3D_PGSQLDBVARIABLE_H

#include <o3d/core/dbvariable.h>

namespace o3d {
namespace pgsql {

/**
 * @brief PgSqlDbVariable
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2019-03-09
 */
class O3D_API PgSqlDbVariable : public DbVariable
{
public:

    PgSqlDbVariable();

    PgSqlDbVariable(
            IntType intType,
            VarType varType,
            UInt8 *data);

    PgSqlDbVariable(
            IntType intType,
            VarType varType,
            UInt32 maxSize);

    virtual ~PgSqlDbVariable();

    //! Set the object null
    virtual void setNull(Bool isNull = True);

    //! Is the object null
    virtual Bool isNull() const;

    //! Get the isNull pointeur.
    virtual UInt8* getIsNullPtr();

    //! Set the length
    virtual void setLength(UInt32 len);

    //! Get the length
    virtual UInt32 getLength() const;

    //! Get the length pointer.
    virtual UInt8* getLengthPtr();

    //! Get the error pointer.
    virtual UInt8* getErrorPtr();

private:

    UInt8 m_isNull;
    UInt8 m_isError;
    unsigned long m_length;
};

} // namespace pgsql
} // namespace o3d

#endif // _O3D_PGSQLDBVARIABLE_H
