// lpccodes.h

#if !defined(_LPCCODES_)
#define _LPCCODES_

#define SVF_CREATE_SECTION          0x2
#define SVF_OPEN_SECTION            0x3
#define SVF_CLOSE_SECTION           0x4
#define SVF_DELETE_SECTION          0x5
#define SVF_RENAME_SECTION          0x6
#define SVF_CREATE_SECTION_LINK     0x7
#define SVF_GET_FIRST_ITEMS         0x8
#define SVF_GET_NEXT_ITEMS          0x9
#define SVF_FETCH_VALUE_BY_ID       0xA
#define SVF_FETCH_VALUE_BY_NAME     0xB
#define SVF_LOCK_SECTION            0xC
#define SVF_PROCESS_BATCH           0xD
#define SVF_DELETE_LONG_BINARY      0xE
#define SVF_LINK_LONG_BINARY        0xF
#define SVF_RENAME_LONG_BINARY      0x10
#define SVF_SECTION_EXISTS          0x11
#define SVF_FLUSH                   0x12 
#define SVF_GET_SECTION_LABEL       0x13
#define SVF_SET_SECTION_LABEL       0x14
#define SVF_DELETE_SECTION_LABEL    0x15
#define SVF_GET_SECTION_LINK_COUNT  0x16
#define SVP_SET_CLIENT_INFO         0x17
#define SVF_OPEN_LONG_BINARY        0x18
#define SVF_CREATE_LONG_BINARY      0x19
#define SVF_GET_DATA                0x1A
#define SVF_PUT_DATA                0x1B
#define SVF_CLOSE_STREAMIN           0x1C
#define SVF_CLOSE_STREAMOUT           0x1D
#define SVF_CREATE_BLOB_TEMP_SECTION  0x1E
#define SVF_COMMIT_BLOB               0x1F
#define SVF_GET_SECTION_SECURITY      0x20
#define SVF_SET_SECTION_SECURITY      0x21

#define SVP_HPARENT          0x1
#define SVP_NAME             0x2
#define SVP_ATTRIBUTES       0x3
#define SVP_NEW_HANDLE       0x4
#define SVP_HBASE            0x5
#define SVP_HSECTION         0x6
#define SVP_PATH             0x7
#define SVP_OLDNAME          0x8
#define SVP_NEWNAME          0x9
#define SVP_OPEN_MODE        0xA
#define SVP_ACCESS_TOKEN     0xB
#define SVP_HSOURCE_BASE     0xC
#define SVP_HSOURCE_SECTION  0xD
#define SVP_HTARGET_SECTION  0xE
#define SVP_SOURCE_PATH      0xF
#define SVP_LINKNAME         0x10
#define SVP_ID_TABLE         0x11
#define SVP_ITEMS            0x11
#define SVP_HASH             0x12
#define SVP_ORDER            0x13
#define SVP_FETCH_VALUE      0x14
#define SVP_LOCK             0x15
#define SVP_BATCH            0x16
#define SVP_COUNT            0x17
#define SVP_BUFFER           0x18
#define SVP_EXIST            0x19
#define SVP_LABEL            0x1A
#define SVP_LINK_COUNT       0x1B
#define SVP_FILTER           0x1C 
#define SVP_OFFSET           0x1D
#define SVP_READONLY            0x1F
#define SVP_CANGETVALUE         0x20
#define SVP_CLIENT_EXE          0x21
#define SVP_CLIENT_USER         0x22
#define SVP_CLIENT_COMPUTER     0x23
#define SVP_CLIENT_THREAD       0x24
#define SVP_HSTREAM             0x25
#define SVP_TEMP_SECTION        0x26
#define SVP_SECURITY_TARGET     0x27
#define SVP_SECURITY_OPERATION  0x28

#define SVP_SECURITY_INFORMATION_FLAGS  0x29
#define SVP_OWNER                       0x2A
#define SVP_PROTECTION_DOMAIN           0x2B

#define SVP_SECURITY_ATTRIBUTES_BASE       0x38
#define SVP_SECURITY_ATTRIBUTES_WHITE_LIST 0x39
#define SVP_SECURITY_ATTRIBUTES_BLACK_LIST 0x3A


#define SVP_SYS_ERRORCODE    0x100
#define SVP_SYS_ERRORITEM    0x101
#define SVP_SYS_ERRORSECTION 0x102
#define SVP_SYS_RETURNVALUE  0x103

#endif // _LPCCODES_