#include <jni.h>

JNIEXPORT jbyteArray JNICALL Java_org_servalproject_succinctdata_xml2succinct
(JNIEnv * env, jobject jobj,
 jstring xmlforminstance,
 jstring recipename,
 jstring succinctpath)
{
  const char *xmldata= (*env)->GetStringUTFChars(env,xmlforminstance,0);
  const char *recipefile= (*env)->GetStringUTFChars(env,recipename,0);
  const char *path= (*env)->GetStringUTFChars(env,succinctpath,0);
  
  unsigned char succinct[1024];
  int succinct_len=0;

  // Trans
  
  jbyteArray result=(*env)->NewByteArray(env, succinct_len);

  (*env)->SetByteArrayRegion(env, result, 0, succinct_len, succinct);
  
  return result;  
}
