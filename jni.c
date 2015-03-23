#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <jni.h>
#include <android/log.h>
 
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "libsmac", __VA_ARGS__))

#include "charset.h"
#include "visualise.h"
#include "arithmetic.h"
#include "packed_stats.h"
#include "smac.h"
#include "recipe.h"
#define MAX_FRAGMENTS 64
int encryptAndFragmentBuffer(unsigned char *in_buffer,int in_len,
			     char *fragments[MAX_FRAGMENTS],int *fragment_count,
			     int mtu,char *publickeyhex);

JNIEXPORT jint JNICALL Java_org_servalproject_succinctdata_jni_updatecsv
(JNIEnv * env, jobject jobj,
 jstring succinctpath,
 jstring rxspooldir,
 jstring outputdir)
{
  LOGI("Line %d",__LINE__);
  const char *path= (*env)->GetStringUTFChars(env,succinctpath,0);
  const char *succinctdatamessage_dir= (*env)->GetStringUTFChars(env,rxspooldir,0);
  const char *output_dir= (*env)->GetStringUTFChars(env,outputdir,0);

  LOGI("Line %d",__LINE__);
  char stats_file[1024];  
  snprintf(stats_file,1024,"%s/smac.dat",path);
  char recipe_dir[1024];  
  snprintf(recipe_dir,1024,"%s",path);

  LOGI("Line %d",__LINE__);

  stats_handle *h=stats_new_handle(stats_file);

  LOGI("Line %d: h=%p from '%s'",__LINE__,h,stats_file);

  char *args[]={"smac","recipe","decompress",recipe_dir,
		(char *)succinctdatamessage_dir,(char *)output_dir,NULL};

  LOGI("Line %d: args: %s %s %s %s %s %s",__LINE__,
       args[0],args[1],args[2],args[3],args[4],args[5]
       );
  recipe_main(6,args,h);
  LOGI("Line %d",__LINE__);

  stats_handle_free(h);
  LOGI("Line %d",__LINE__);

  return 0;
}

jobjectArray error_message(JNIEnv * env, char *message)
{
  LOGI("%s",message);
      
  jobjectArray result=
    (jobjectArray)(*env)->NewObjectArray(env,1,
					 (*env)->FindClass(env,"java/lang/String"),
					 (*env)->NewStringUTF(env,""));
  (*env)->SetObjectArrayElement(env,result,0,(*env)->NewStringUTF(env,message));

  return result;
}


JNIEXPORT jobjectArray JNICALL Java_org_servalproject_succinctdata_jni_xml2succinctfragments
(JNIEnv * env, jobject jobj,
 jstring xmlforminstance,
 jstring formname,
 jstring formversion,
 jstring succinctpath,
 jint mtu)
{
  const char *xmldata= (*env)->GetStringUTFChars(env,xmlforminstance,0);
  const char *formname_c= (*env)->GetStringUTFChars(env,formname,0);
  const char *formversion_c= (*env)->GetStringUTFChars(env,formversion,0);
  const char *path= (*env)->GetStringUTFChars(env,succinctpath,0);
  
  char stripped[8192];
  unsigned char succinct[1024];
  int succinct_len=0;
  char filename[1024];

  // Read public key hex
  snprintf(filename,1024,"%s/%s.%s.publickey",path,formname_c,formversion_c);
  LOGI("Opening recipient public key file %s",filename);
  char publickeyhex[1024]="";
  {
    FILE *f=fopen(filename,"r");
    if (!f) {
      char message[1024];
      snprintf(message,1024,"Failed to open public key file");
      return error_message(env,message);
    } else {
      int r=fread(publickeyhex,1,1023,f);
      if (r<64) {
	char message[1024];
	snprintf(message,1024,"Failed to read from public key file");
	return error_message(env,message);
      }
      publickeyhex[r]=0;
    }
    while(publickeyhex[strlen(publickeyhex)-1]<' ') publickeyhex[strlen(publickeyhex)-1]=0;
    fclose(f);
  }
  
  // Read recipe file
  snprintf(filename,1024,"%s/%s.%s.recipe",path,formname_c,formversion_c);
  LOGI("Opening recipe file %s",filename);
  struct recipe *recipe=recipe_read_from_file(filename);
  
  if (!recipe) {
    char message[1024];
    snprintf(message,1024,"Could not read recipe file %s",filename);
    return error_message(env,message);
  }

  // Transform XML to stripped data first.
  int stripped_len=xml2stripped(formname_c,xmldata,strlen(xmldata),stripped,8192);

  if (stripped_len>0) {
    // Produce succinct data

    // Get stats handle
    char filename[1024];
    snprintf(filename,1024,"%s/smac.dat",path);
    stats_handle *h=stats_new_handle(filename);

    if (!h) {
      recipe_free(recipe);
      char message[1024];
      snprintf(message,1024,"Could not read SMAC stats file %s",filename);
      return error_message(env,message);
    }

    // Compress stripped data to form succinct data
    succinct_len=recipe_compress(h,recipe,stripped,stripped_len,succinct,sizeof(succinct));

    // Clean up after ourselves
    stats_handle_free(h);
    recipe_free(recipe);
    snprintf(filename,1024,"%s/%s.%s.recipe",path,formname_c,formversion_c);

    if (succinct_len<1) {
      recipe_free(recipe);
      char message[1024];
      snprintf(message,1024,"recipe_compess failed with recipe file %s. h=%p, recipe=%p, stripped_len=%d",filename,h,recipe,stripped_len);
      return error_message(env,message);
    }
  } else {
    recipe_free(recipe);
    char message[1024];
    snprintf(message,1024,"Failed to strip XML using recipe file %s.",filename);
    return error_message(env,message);
  }

  char *fragments[MAX_FRAGMENTS];
  int fragment_count=0;
  encryptAndFragmentBuffer(succinct,succinct_len,fragments,&fragment_count,mtu,
			   publickeyhex);
  
  jobjectArray result=
    (jobjectArray)(*env)->NewObjectArray(env,fragment_count,
				      (*env)->FindClass(env,"java/lang/String"),
         (*env)->NewStringUTF(env,""));
  for(int i=0;i<fragment_count;i++) {
    (*env)->SetObjectArrayElement(env,result,i,(*env)->NewStringUTF(env,fragments[i]));
    free(fragments[i]);
  }
    
  return result;  
}

