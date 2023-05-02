/**
 * @file pgsqldbvariable.cpp
 * @brief 
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2019-03-09
 * @copyright Copyright (c) 2001-2019 Dream Overflow. All rights reserved.
 * @details 
 */

#include "o3d/pgsql/pgsqldbvariable.h"

using namespace o3d;
using namespace o3d::pgsql;

PgSqlDbVariable::PgSqlDbVariable()
{

}

PgSqlDbVariable::PgSqlDbVariable(
        DbVariable::IntType intType,
        DbVariable::VarType varType,
        UInt8 *data) :
    DbVariable(intType, varType, data)
{
    // null
    if (!data)
    {
        m_object = m_objectPtr = nullptr;
        m_isNull = True;

        return;
    }
}

PgSqlDbVariable::PgSqlDbVariable(
        DbVariable::IntType intType,
        DbVariable::VarType varType,
        UInt32 maxSize) :
    DbVariable(intType, varType, maxSize)
{

}

PgSqlDbVariable::~PgSqlDbVariable()
{

}

void PgSqlDbVariable::setNull(Bool isNull)
{
    m_isNull = (UInt8)isNull;
}

Bool PgSqlDbVariable::isNull() const
{
    return m_isNull != 0;
}

UInt8 *PgSqlDbVariable::getIsNullPtr()
{
    return (UInt8*)&m_isNull;
}

void PgSqlDbVariable::setLength(UInt32 len)
{
    m_length = len;
}

UInt32 PgSqlDbVariable::getLength() const
{
    return m_length;
}

UInt8 *PgSqlDbVariable::getLengthPtr()
{
    return (UInt8*)&m_length;
}

UInt8 *PgSqlDbVariable::getErrorPtr()
{
    return (UInt8*)&m_isError;
}
