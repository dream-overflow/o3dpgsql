/**
 * @file pgsql.h
 * @brief 
 * @author Frederic SCHERMA (frederic.scherma@dreamoverflow.org)
 * @date 2019-03-09
 * @copyright Copyright (c) 2001-2019 Dream Overflow. All rights reserved.
 * @details 
 */

#ifndef _O3D_PGSQL_H
#define _O3D_PGSQL_H

#include <objective3dconfig.h>

namespace o3d {

// If no object export/import mode defined suppose IMPORT
#if !defined(O3D_PGSQL_EXPORT_DLL) && !defined(O3D_PGSQL_STATIC_LIB)
    #ifndef O3D_PGSQL_IMPORT_DLL
        #define O3D_PGSQL_IMPORT_DLL
    #endif
#endif

//---------------------------------------------------------------------------------------
// API define depend on OS and dynamic library exporting type
//---------------------------------------------------------------------------------------

#if (defined(O3D_UNIX) || defined(O3D_MACOSX) || defined(SWIG))
  #if __GNUC__ >= 4
    #define O3D_PGSQL_API __attribute__ ((visibility ("default")))
    #define O3D_PGSQL_API_PRIVATE __attribute__ ((visibility ("hidden")))
    #define O3D_PGSQL_API_TEMPLATE
  #else
    #define O3D_PGSQL_API
    #define O3D_PGSQL_API_PRIVATE
    #define O3D_PGSQL_API_TEMPLATE
  #endif
#elif defined(O3D_WINDOWS)
    // export DLL
    #ifdef O3D_PGSQL_EXPORT_DLL
        #define O3D_PGSQL_API __declspec(dllexport)
        #define O3D_PGSQL_API_PRIVATE
        #define O3D_PGSQL_API_TEMPLATE
    #endif
    // import DLL
    #ifdef O3D_PGSQL_IMPORT_DLL
        #define O3D_PGSQL_API __declspec(dllimport)
        #define O3D_PGSQL_API_PRIVATE
        #define O3D_PGSQL_API_TEMPLATE
    #endif
    // static (no DLL)
    #ifdef O3D_NET_STATIC_LIB
        #define O3D_PGSQL_API
        #define O3D_PGSQL_API_PRIVATE
        #define O3D_PGSQL_API_TEMPLATE
    #endif
#endif

} // namespace o3d

#endif // _O3D_PGSQL_H
