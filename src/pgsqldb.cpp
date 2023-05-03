/**
 * @file pgsqldb.cpp
 * @brief 
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2019-09-03
 * @copyright Copyright (c) 2001-2019 Dream Overflow. All rights reserved.
 * @details 
 */

#include "o3d/pgsql/pgsqldb.h"
#include "o3d/pgsql/pgsqlexception.h"
#include "o3d/pgsql/pgsqldbvariable.h"

#include <o3d/core/application.h>
#include <o3d/core/objects.h>

#include <stdio.h>
#include <netinet/in.h>

using namespace o3d;
using namespace o3d::pgsql;

static UInt32 ms_pgSqlLibRefCount = 0;
static Bool ms_pgSqlLibState = False;

// workaround for postgres defining their OIDs in a private header file
#define QBOOLOID 16
#define QINT8OID 20
#define QINT2OID 21
#define QINT4OID 23
#define QNUMERICOID 1700
#define QFLOAT4OID 700
#define QFLOAT8OID 701
#define QABSTIMEOID 702
#define QRELTIMEOID 703
#define QDATEOID 1082
#define QTIMEOID 1083
#define QTIMETZOID 1266
#define QTIMESTAMPOID 1114
#define QTIMESTAMPTZOID 1184
#define QOIDOID 2278
#define QBYTEAOID 17
#define QREGPROCOID 24
#define QXIDOID 28
#define QCIDOID 29

#define QBITOID 1560
#define QVARBITOID 1562

// add json

#define SMJSONOID 114
#define SMJSONBOID 3802

// add arrays

#define SMBOOL_ARRAYOID 1000
#define SMINT8_ARRAYOID 1016
#define SMINT2_ARRAYOID 1005
#define SMINT4_ARRAYOID 1007
#define SMNUMERIC_ARRAYOID 1231
#define SMFLOAT4_ARRAYOID 1021
#define SMFLOAT8_ARRAYOID 1022
#define SMABSTIME_ARRAYOID 1023
#define SMRELTIME_ARRAYOID 1024
#define SMDATE_ARRAYOID 1182
#define SMTIME_ARRAYOID 1183
#define SMTIMETZ_ARRAYOID 1270
#define SMTIMESTAMP_ARRAYOID 1115
#define SMTIMESTAMPTZ_ARRAYOID 1185
#define SMBYTEA_ARRAYOID 1001
#define SMREGPROC_ARRAYOID 1008
#define SMXID_ARRAYOID 1011
#define SMCID_ARRAYOID 1012
#define SMJSON_ARRAYOID 199
#define SMJSONB_ARRAYOID 3807

#define SMVARCHAR_ARRAYOID 1015
#define SMTEXT_ARRAYOID 1009

// STMT https://www.postgresql.org/docs/9.3/sql-prepare.html

void PgSql::init()
{
    if (!ms_pgSqlLibState) {
        Application::registerObject("o3d::PgSql", nullptr);
        ms_pgSqlLibState = True;
        ms_pgSqlLibRefCount = 0;
    }
}

void PgSql::quit()
{
    if (ms_pgSqlLibState) {
        if (ms_pgSqlLibRefCount != 0) {
            O3D_ERROR(E_InvalidOperation("Trying to quit pgsql library but some database still exists"));
        }

        Application::unregisterObject("o3d::PgSql");
        ms_pgSqlLibState = False;
    }
}

//! Default ctor
PgSqlDb::PgSqlDb() :
    Database(),
    m_pDB(nullptr)
{
    if (!ms_pgSqlLibState) {
        O3D_ERROR(E_InvalidPrecondition("PgSql::init() must be called before"));
    }

    ++ms_pgSqlLibRefCount;
}

PgSqlDb::~PgSqlDb()
{
    disconnect();
    --ms_pgSqlLibRefCount;
}

// Connect to a database
Bool PgSqlDb::connect(
        const String &host,
        UInt32 port,
        const String &database,
        const String &user,
        const String &password,
        Bool keepPassord)
{
    Int32 pos;

    if (!port) {
        port = 5432;  // default is not
    }

    m_host = host;
    m_database = database;
    m_user = user;

    if (keepPassord) {
        m_password = password;
	}

    if ((pos = host.find(':')) != -1) {
        m_host.truncate(pos);
        port = host.sub(pos+1).toUInt32();
    }

    String connInfo = String("hostaddr={0} port={1} dbname={2} user={3} password={4} keepalives=1")
                           .arg(host).arg(port).arg(database).arg(user).arg(password);
    // PGresult *res;
    m_pDB = PQconnectdb(connInfo.toUtf8().getData());
    O3D_ASSERT(m_pDB != nullptr);

    if (PQstatus(m_pDB) != CONNECTION_OK) {
        const char* err = PQerrorMessage(m_pDB);
        o3d::String msg;
        msg.fromUtf8(err);

        O3D_ERROR(o3d::pgsql::E_PgSqlError(msg));
    }

    O3D_MESSAGE("Successfuly connected to the PgSql database");

    m_isConnected = True;

    return True;
}

// Disconnect from the database server
void PgSqlDb::disconnect()
{
    if (m_isConnected) {
        m_isConnected = False;
    }

    if (m_pDB) {
        PQfinish(m_pDB);
        m_pDB = nullptr;
    }
}

// Try to maintain the connection established
void PgSqlDb::pingConnection()
{
    if (m_pDB) {
        // mysql_ping(m_pDB);
    }
}

// Instanciate a new DbQuery object
DbQuery* PgSqlDb::newDbQuery(const String &name, const CString &query)
{
    return new PgSqlQuery(m_pDB, name, query);
}

// Virtual destructor
PgSqlQuery::~PgSqlQuery()
{
    for (Int32 i = 0; i < m_inputs.getSize(); ++i) {
        deletePtr(m_inputs[i]);
    }

    for (Int32 i = 0; i < m_outputs.getSize(); ++i) {
        deletePtr(m_outputs[i]);
    }

    if (m_pRes) {
        PQclear(m_pRes);
        m_pRes = nullptr;
    }
}

void PgSqlQuery::setArrayUInt8(UInt32 attr, const ArrayUInt8 &v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    if (v.getSize() < (1 << 8)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_ARRAY_UINT8, DbVariable::TINY_ARRAY, (UInt8*)&v);
    } else if (v.getSize() < (1 << 16)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_ARRAY_UINT8, DbVariable::ARRAY, (UInt8*)&v);
    } else if (v.getSize() < (1 << 24)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_ARRAY_UINT8, DbVariable::MEDIUM_ARRAY, (UInt8*)&v);
    } else {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_ARRAY_UINT8, DbVariable::LONG_ARRAY, (UInt8*)&v);
    }

   /* DbVariable &var = *m_inputs[attr];

    enum_field_types dbtype = (enum_field_types)0;
    unsigned long dbsize = 0;

    mapType(var.getType(), dbtype, dbsize);

    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
    m_param_bind[attr].buffer_length = var.getObjectSize();

    m_param_bind[attr].is_null = 0;

    var.setLength(var.getObjectSize());
    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();
*/
    m_needBind = True;
}

void PgSqlQuery::setSmartArrayUInt8(UInt32 attr, const SmartArrayUInt8 &v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    if (v.getSizeInBytes() < (1 << 8)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_SMART_ARRAY_UINT8, DbVariable::TINY_ARRAY, (UInt8*)&v);
    } else if (v.getSizeInBytes() < (1 << 16)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_SMART_ARRAY_UINT8, DbVariable::ARRAY, (UInt8*)&v);
    } else if (v.getSizeInBytes() < (1 << 24)) {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_SMART_ARRAY_UINT8, DbVariable::MEDIUM_ARRAY, (UInt8*)&v);
    } else {
        m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_SMART_ARRAY_UINT8, DbVariable::LONG_ARRAY, (UInt8*)&v);
    }

    DbVariable &var = *m_inputs[attr];
/*
    enum_field_types dbtype = (enum_field_types)0;
    unsigned long dbsize = 0;

    mapType(var.getType(), dbtype, dbsize);

    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
    m_param_bind[attr].buffer_length = var.getObjectSize();

    m_param_bind[attr].is_null = 0;

    var.setLength(var.getObjectSize());
    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();
*/
    m_needBind = True;
}

void PgSqlQuery::setInStream(UInt32 attr, const InStream &v)
{
    O3D_ERROR(E_InvalidOperation("Not yet implemented"));
}

void PgSqlQuery::setBool(UInt32 attr, Bool v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_BOOL, DbVariable::BOOLEAN, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];
/*
    enum_field_types dbtype = (enum_field_types)0;
    unsigned long dbsize = 0;

    mapType(var.getType(), dbtype, dbsize);

    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
    m_param_bind[attr].buffer_length = var.getObjectSize();

    m_param_bind[attr].is_null = 0;

    var.setLength(dbsize);
    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();
*/
    m_needBind = True;
}

void PgSqlQuery::setInt32(UInt32 attr, Int32 v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_INT32, DbVariable::INT32, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];
/*
    enum_field_types dbtype = (enum_field_types)0;
    unsigned long dbsize = 0;

    mapType(var.getType(), dbtype, dbsize);

    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
    m_param_bind[attr].buffer_length = var.getObjectSize();

    //m_param_bind[attr].is_null_value = False; @see if we support null value
    m_param_bind[attr].is_null = 0;

    var.setLength(dbsize);
    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();
*/
    m_needBind = True;
}

void PgSqlQuery::setUInt32(UInt32 attr, UInt32 v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_INT32, DbVariable::UINT32, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];
/*
    enum_field_types dbtype = (enum_field_types)0;
    unsigned long dbsize = 0;

    mapType(var.getType(), dbtype, dbsize);

    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;

    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
    m_param_bind[attr].buffer_length = var.getObjectSize();

    m_param_bind[attr].is_null = 0;
    m_param_bind[attr].is_unsigned = True;

    var.setLength(dbsize);
    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();
*/
    m_needBind = True;
}

void o3d::pgsql::PgSqlQuery::setFloat(UInt32 attr, Float v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_FLOAT, DbVariable::FLOAT32, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];

    // @todo for STMT

    m_needBind = True;
}

void o3d::pgsql::PgSqlQuery::setDouble(UInt32 attr, Double v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_DOUBLE, DbVariable::FLOAT64, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];

    // @todo for STMT

    m_needBind = True;
}

void PgSqlQuery::setCString(UInt32 attr, const CString &v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_CSTRING, DbVariable::VARCHAR, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];

//    int dbtype = 0;
//    unsigned long dbsize = 0;

//    mapType(var.getType(), dbtype, dbsize);

//    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

//    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
//    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
//    m_param_bind[attr].buffer_length = var.getObjectSize()-1;

//    m_param_bind[attr].is_null = 0;

//    var.setLength(var.getObjectSize()-1);
//    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();

    m_needBind = True;
}

void PgSqlQuery::setDate(UInt32 attr, const Date &v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_DATE, DbVariable::TIMESTAMP, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];

//    MYSQL_TIME *mysqlTime = (MYSQL_TIME*)var.getObjectPtr();
//    memset(mysqlTime, 0, sizeof(MYSQL_TIME));

//    mysqlTime->day = v.mday + 1;
//    mysqlTime->hour = 0;
//    mysqlTime->minute = 0;
//    mysqlTime->month = v.month + 1;
//    mysqlTime->second = 0;
//    mysqlTime->time_type = MYSQL_TIMESTAMP_DATETIME;
//    mysqlTime->year = v.year;

//    enum_field_types dbtype = (enum_field_types)0;
//    unsigned long dbsize = 0;

//    mapType(var.getType(), dbtype, dbsize);

//    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

//    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
//    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
//    m_param_bind[attr].buffer_length = dbsize;

//    m_param_bind[attr].is_null = 0;

//    var.setLength(dbsize);
//    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();

//    m_needBind = True;
}

void PgSqlQuery::setTimestamp(UInt32 attr, const DateTime &v)
{
    if (attr >= m_numParam) {
        O3D_ERROR(E_IndexOutOfRange("Input attribute id"));
    }

    if (m_inputs[attr]) {
        deletePtr(m_inputs[attr]);   
    }

    m_inputs[attr] = new PgSqlDbVariable(DbVariable::IT_DATETIME, DbVariable::TIMESTAMP, (UInt8*)&v);
    DbVariable &var = *m_inputs[attr];

//    MYSQL_TIME *mysqlTime = (MYSQL_TIME*)var.getObjectPtr();
//    memset(mysqlTime, 0, sizeof(MYSQL_TIME));

//    mysqlTime->day = v.mday + 1;
//    mysqlTime->hour = v.hour;
//    mysqlTime->minute = v.minute;
//    mysqlTime->month = v.month + 1;
//    mysqlTime->second = v.second;
//    mysqlTime->time_type = MYSQL_TIMESTAMP_DATETIME;
//    mysqlTime->year = v.year;

//    enum_field_types dbtype = (enum_field_types)0;
//    unsigned long dbsize = 0;

//    mapType(var.getType(), dbtype, dbsize);

//    memset(&m_param_bind[attr], 0, sizeof(MYSQL_BIND));

//    m_param_bind[attr].buffer_type = (enum_field_types)dbtype;
//    m_param_bind[attr].buffer = (void*)var.getObjectPtr();
//    m_param_bind[attr].buffer_length = dbsize;

//    m_param_bind[attr].is_null = 0;

//    var.setLength(dbsize);
//    m_param_bind[attr].length = (unsigned long*)var.getLengthPtr();

//    m_needBind = True;
}

UInt32 PgSqlQuery::getOutAttr(const CString &name)
{
    auto it = m_outputNames.find(name);
    if (it != m_outputNames.end()) {
        return it->second;
    } else {
        O3D_ERROR(E_InvalidParameter(o3d::String("Unknown output attribute name ") + name));
    }
}

const DbVariable &PgSqlQuery::getOut(const CString &name) const
{
    auto it = m_outputNames.find(name);
    if (it != m_outputNames.end()) {
        return *m_outputs[it->second];
    } else {
        O3D_ERROR(E_InvalidParameter(o3d::String("Unknown output attribute name ") + name));
    }
}

const DbVariable &PgSqlQuery::getOut(UInt32 attr) const
{
    if (attr < (UInt32)m_outputs.getSize()) {
        return *m_outputs[attr];
    } else {
        O3D_ERROR(E_IndexOutOfRange("Output attribute is out of range"));
    }
}

PgSqlQuery::PgSqlQuery(PGconn *pDb, const String &name, const CString &query) :
    m_name(name),
    m_query(query),
    m_numParam(0),
    m_numRow(0),
    m_currRow(0),
    m_pDB(pDb),
    m_pRes(nullptr)
{
    prepareQuery();
}

// Prepare the query. Can do nothing if not preparation is needed
void PgSqlQuery::prepareQuery()
{
    O3D_ASSERT(m_pDB != nullptr);
    if (m_pDB) {
        // @todo later stmt, but for no simple

        // inputs
        o3d::String str(m_query);

        m_numParam = str.count('$');   // probably no sufficient
        m_inputs.setSize(m_numParam);
        // m_outputs
        // m_outputNames

        // https://docs.postgresql.fr/12/libpq-exec.html
        // PGresult *PQprepare(PGconn *conn, const char *stmtName, const char *query, int nParams, const Oid *paramTypes);
        // PGresult *PQdescribePrepared(PGconn *conn, const char *stmtName);

        m_needBind = True;
	}
}

// Unbind the current bound DbAttribute
void PgSqlQuery::unbind()
{

}

// Execute the query on the current bound DbAttribute and store the result in the DbAttribute
void PgSqlQuery::execute()
{
    if (m_pRes) {
        PQclear(m_pRes);
        m_pRes = nullptr;
    }

    // const Oid *paramTypes;
    char **paramValues = new char*[m_numParam];
    Oid *paramTypes = nullptr;  // new Oid[m_numParam];    // let the backend deduce param type
    int *paramLengths = nullptr;  // new int[m_numParam];  // if all texts no need
    int *paramFormats = nullptr;  // default to all text params

    for (o3d::Int32 i = 0; i < m_inputs.getSize(); ++i) {
        if (m_inputs[i]->getIntType() == DbVariable::IT_CSTRING) {
            CString v = m_inputs[i]->asCString();

            // paramTypes[i] = 0;
            paramValues[i] = new char[v.length()+1];
            // paramLengths[i] = -1;
            memcpy(paramValues[i], v.getData(), v.length()+1);

        } else if (m_inputs[i]->getIntType() == DbVariable::IT_INT32) {
            o3d::Int32 v = m_inputs[i]->asInt32();

            // paramTypes[i] = 0;
            paramValues[i] = new char[4];
            // paramLengths[i] = 4;
            memcpy(paramValues[i], &v, 4);

        } else if (m_inputs[i]->getIntType() == DbVariable::IT_ARRAY_UINT8) {

            // @todo

        } else if (m_inputs[i]->getIntType() == DbVariable::IT_FLOAT) {
            o3d::Float v = m_inputs[i]->asFloat();

            // paramTypes[i] = 0;
            paramValues[i] = new char[4];
            // paramLengths[i] = 4;
            memcpy(paramValues[i], &v, 4);

        } else if (m_inputs[i]->getIntType() == DbVariable::IT_DOUBLE) {
            o3d::Double v = m_inputs[i]->asDouble();

            o3d::Int32 l = snprintf(nullptr, 0, "%g", v);
            Char *str = new Char[l+1];
            sprintf(str, "%g", v);

            // paramTypes[i] = 0;
            paramValues[i] = str;
            // paramLengths[i] = -1;

        } else {
            // @todo others ...
        }
    }

    // PGresult *res = PQexecPrepared(...)
    m_pRes = PQexecParams(m_pDB,
                       m_query.getData(),
                       m_numParam,
                       paramTypes,
                       paramValues,
                       paramLengths,
                       paramFormats,
                       1);      // ask for binary results

    if (PQresultStatus(m_pRes) != PGRES_TUPLES_OK) {
        o3d::String msg;
        msg.fromUtf8(PQerrorMessage(m_pDB));
        O3D_ERROR(o3d::pgsql::E_PgSqlError(msg));
        PQclear(m_pRes);
        m_pRes = nullptr;
    }

    m_currRow = 0;
    m_numRow = PQntuples(m_pRes);

    // bind output types
    o3d::Int32 nCols = PQnfields(m_pRes);

    UInt32 maxSize;
    DbVariable::IntType intType;
    DbVariable::VarType varType;

    o3d::Bool initial = m_outputNames.empty();

    // int PQfnumber(const PGresult *res,const char *column_name); inverse de PQfname
    for (o3d::Int32 col = 0; col < nCols; ++col) {
        char* fname = PQfname(m_pRes, col);
        if (fname == nullptr) {
            continue;
        }

        if (initial) {
            m_outputNames.insert(std::make_pair(fname, col));
        }

        Oid pgsqltype = PQftype(m_pRes, col);
        int size = PQfsize(m_pRes, col);
        int mod = PQfmod(m_pRes, col);

        unmapType(pgsqltype, maxSize, intType, varType);
        // printf("%s : %i = %i(%i) (%i %i)\n", fname, col, pgsqltype, intType, size, mod);

        if (m_outputs[col] == nullptr) {
            // only the first time
            m_outputs[col] = new PgSqlDbVariable(intType, varType, maxSize);
        }
    }

    // free input parameters
    for (o3d::UInt32 i = 0; i < m_numParam; ++i) {
        deletePtr(paramValues[i]);
    }

    deletePtr(paramValues);
}

void PgSqlQuery::update()
{
//    O3D_ASSERT(m_stmt != nullptr);

//    if (m_stmt) {
//        m_numRow = 0;
//        m_currRow = 0;

//        // bind if necessary
//        if (m_needBind) {
//            if (mysql_stmt_bind_param(m_stmt, &m_param_bind[0]) != 0) {
//                O3D_ERROR(E_MySqlError(mysql_stmt_error(m_stmt)));
//            }

//            m_needBind = False;
//        }

//        if (mysql_stmt_execute(m_stmt) != 0) {
//            O3D_ERROR(E_MySqlError(mysql_stmt_error(m_stmt)));
//        }

//        m_numRow = mysql_stmt_affected_rows(m_stmt);
//    }
}

UInt32 PgSqlQuery::getNumRows()
{
    return m_numRow;
}

UInt64 PgSqlQuery::getGeneratedKey() const
{
//    O3D_ASSERT(m_stmt != nullptr);
//    if (m_stmt) {
//        UInt64 id = mysql_stmt_insert_id(m_stmt);
//        return id;
//    }

    return 0;
}

// Fetch the results (outputs values) into the DbAttribute. Can be called in a while for each entry of the result.
Bool PgSqlQuery::fetch()
{
    if (m_pRes) {
         // int PQfnumber(const PGresult *res,const char *column_name); inverse de PQfname
        for (o3d::Int32 i = 0; i < m_outputs.getSize(); ++i) {
            DbVariable &var = *m_outputs[i];
            char* value = PQgetvalue(m_pRes, m_currRow, i);
            int len = PQgetlength(m_pRes, m_currRow, i);

            if (var.getIntType() == DbVariable::IT_INT32) {
                // int32
                var.setInt32(ntohl(*((int32_t *) value)));
                // printf("int32 %i = %i\n", i, var.asInt32());

            } else if (var.getIntType() == DbVariable::IT_INT64) {
                // int64
                if (System::getNativeByteOrder() != System::ORDER_BIG_ENDIAN) {
                    System::swapBytes8(value);
                }
                var.setInt64(*(o3d::Int64 *) value);
                // printf("int64 %i %lli\n", i, var.asInt64());

            } else if (var.getIntType() == DbVariable::IT_FLOAT) {
                // double
                if (System::getNativeByteOrder() != System::ORDER_BIG_ENDIAN) {
                    System::swapBytes4(value);
                }
                var.setFloat(*(o3d::Float*) value);
                // printf("float %i %f\n", i, var.asFloat());

            } else if (var.getIntType() == DbVariable::IT_DOUBLE) {
                // double
                if (System::getNativeByteOrder() != System::ORDER_BIG_ENDIAN) {
                    System::swapBytes8(value);
                }
                var.setDouble(*(o3d::Double *) value);
                // printf("double %i %f\n", i, var.asDouble());

            } else if (var.getIntType() == DbVariable::IT_ARRAY_CHAR) {
                // string
                ArrayChar *array = (ArrayChar*)var.getObject();
                // printf("str %i %i %s\n", i, len, value);

                // add a terminal zero
                array->setSize(len+1);
                memcpy(array->getData(), value, len);
                (*array)[array->getSize()-1] = 0;

            } else if (var.getIntType() == DbVariable::IT_CSTRING) {
                // string
                // printf("str %i %i %s\n", i, len, value);
                var.setCString(value);
            }
            // array
//            else if (var.getIntType() == DbVariable::IT_ARRAY_UINT8) {
//                ArrayUInt8 *array = (ArrayUInt8*)var.getObject();
//                array->setSize(var.getLength());
//			}
//            // date
//            else if (var.getIntType() == DbVariable::IT_DATE) {
//                Date *date = (Date*)var.getObject();
//                MYSQL_TIME *mysqlTime = (MYSQL_TIME*)var.getObjectPtr();
//                date->day = Day(mysqlTime->day - 1);
//                date->month = Month(mysqlTime->month - 1);
//                date->year = mysqlTime->year;
//            }
//            // datetime
//            else if (var.getIntType() == DbVariable::IT_DATETIME) {
//                DateTime *datetime = (DateTime*)var.getObject();
//                MYSQL_TIME *mysqlTime = (MYSQL_TIME*)var.getObjectPtr();
//                datetime->day = Day(mysqlTime->day - 1);
//                datetime->hour = mysqlTime->hour;
//                datetime->minute = mysqlTime->minute;
//                datetime->month = Month(mysqlTime->month - 1);
//                datetime->second = mysqlTime->second;
//                datetime->year = mysqlTime->year;
//            }
        }

        ++m_currRow;
        return True;
    }

    return False;
}

UInt32 PgSqlQuery::tellRow()
{
    if (m_pRes) {
        return m_currRow;
    } else {
        return 0;
    }
}

void PgSqlQuery::seekRow(UInt32 row)
{
    if (row >= m_numRow) {
        O3D_ERROR(E_IndexOutOfRange("Row number"));
    }

    if (m_pRes) {
        // mysql_stmt_data_seek(m_stmt, row);
        m_currRow = row;
    }
}

void PgSqlQuery::unmapType(Oid pgsqltype,
        UInt32 &maxSize,
        DbVariable::IntType &intType,
        DbVariable::VarType &varType)
{
    switch (pgsqltype) {
    case QBOOLOID:
        intType = DbVariable::IT_BOOL;
        varType = DbVariable::BOOLEAN;
        maxSize = 1;
        break;

//    case MYSQL_TYPE_TINY:
//        intType = DbVariable::IT_INT8;
//        varType = DbVariable::INT8;
//        maxSize = 1;
//        break;

//    case MYSQL_TYPE_SHORT:
//        intType = DbVariable::IT_INT16;
//        varType = DbVariable::INT16;
//        maxSize = 2;
//        break;

//    case MYSQL_TYPE_INT24:
//        intType = DbVariable::IT_INT32;
//        varType = DbVariable::INT32;
//        maxSize = 4;
//        break;

    case QINT2OID:
    case QINT4OID:
    case QOIDOID:
    case QREGPROCOID:
    case QXIDOID:
    case QCIDOID:
        intType = DbVariable::IT_INT32;
        varType = DbVariable::INT32;
        maxSize = 4;
        break;

    case QINT8OID:
        intType = DbVariable::IT_INT64;
        varType = DbVariable::INT64;
        maxSize = 8;
        break;

//    case MYSQL_TYPE_FLOAT:
//        intType = DbVariable::IT_FLOAT;
//        varType = DbVariable::FLOAT32;
//        maxSize = 4;
//        break;

    case QNUMERICOID:
    case QFLOAT4OID:
    case QFLOAT8OID:
        intType = DbVariable::IT_DOUBLE;
        varType = DbVariable::FLOAT64;
        maxSize = 8;
        break;

//    case SMVARCHAR_ARRAYOID:
//    case SMTEXT_ARRAYOID:
//        intType = DbVariable::IT_ARRAY_CHAR;
//        varType = DbVariable::ARRAY;
//        maxSize = 256;
//        break;

//    case MYSQL_TYPE_VAR_STRING:
//        intType = DbVariable::IT_ARRAY_CHAR;
//        varType = DbVariable::ARRAY;
//        maxSize = 256;
//        break;

//    case MYSQL_TYPE_TINY_BLOB:
//        intType = DbVariable::IT_ARRAY_UINT8;
//        varType = DbVariable::TINY_ARRAY;
//        maxSize = 256;
//        break;

//    case MYSQL_TYPE_BLOB:
//        intType = DbVariable::IT_ARRAY_UINT8;
//        varType = DbVariable::ARRAY;
//        maxSize = 4096;
//        break;

//    case MYSQL_TYPE_MEDIUM_BLOB:
//        intType = DbVariable::IT_ARRAY_UINT8;
//        varType = DbVariable::MEDIUM_ARRAY;
//        maxSize = 4096;
//        break;

    case QBYTEAOID:
        intType = DbVariable::IT_ARRAY_UINT8;
        varType = DbVariable::LONG_ARRAY;
        maxSize = 4096;
        break;

//    case QABSTIMEOID:
//    case QRELTIMEOID:
//    case QDATEOID:
//        intType = DbVariable::IT_DATE
//        varType = DbVariable::TIMESTAMP;
//        maxSize = xxx
//        break;

//    case QTIMEOID:
//    case QTIMETZOID:
//        intType = DbVariable::IT_DATETIME
//        varType = DbVariable::TIMESTAMP;
//        maxSize = xxx
//        break;

//    case QTIMESTAMPOID:
//    case QTIMESTAMPTZOID:
//        intType = DbVariable::IT_DATE;
//        varType = DbVariable::TIMESTAMP;
//        maxSize = xxx
//        break;

    case 1043:
    default:
        intType = DbVariable::IT_ARRAY_CHAR;
        varType = DbVariable::ARRAY;
        maxSize = 256;
        break;
    };
}
