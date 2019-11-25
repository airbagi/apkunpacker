/*
 * Header file for the definitions of packers/protectors
 *
 * Tim "diff" Strazzere <strazz@gmail.com>
 * modified by Zend (2019)
 */

typedef struct {
  char* name;
  char* description;
  char* filter;
  char* marker;
} packer;

static packer packers[] = {

  // APKProtect
  {
    "APKProtect v1->5",
    "APKProtect generialized detection",
    // This is actually the filter APKProtect uses itself for finding it's own odex to modify
    ".apk@",
    "/libAPKProtect"
  },

  // Bangcle (??) or something equally silly
  {
    "Bangcle (?) silly version",
    "Something silly used by malware",
    "/app_lib/classes.dex",
    "/app_lib/"
  },

  {
    "Bangle SGMain version",
    "A seemingly one off of Bangcle (SecNeo)",
    "/libsgmain_",
    "/libsgmain.so"
  },

  /*
  // Jaigu
  {
    "Jaigu",
    "Jaigu generic unpacker",
    "/.jiagu/libjiagu.so",
    // Always after the last dalvik cache
  },
  */

  // LIAPP
  {
    "LIAPP 'Egg' (v1->?)",
    "LockIn APP (lockincomp.com)",
    "LIAPPEgg.dex",
    "/LIAPPEgg"
  },

  // Qihoo 'Monster'
  {
    "Qihoo 'Monster' (v1->?)",
    "Qihoo unknown version, code named 'monster'",
    "monster.dex",
    "/libprotectClass"
  },

  {
    "TenCent 2018 packer",
    "TenCent v 2018+",
    "/data/dalvik-cache/x86/data@app@",
    "/data/dalvik-cache/x86/data@app@%s"
  }
};
