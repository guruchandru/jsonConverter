#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <msgpack.h>
#include <cJSON.h>
#include <base64.h>
#include "jsonConverter.h"
/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void packJsonString( cJSON *item, msgpack_packer *pk );
static void packJsonNumber( cJSON *item, msgpack_packer *pk );
static void packJsonArray( cJSON *item, msgpack_packer *pk );
static void packJsonObject( cJSON *item, msgpack_packer *pk );
static void packJsonBool(cJSON *item, msgpack_packer *pk, bool value);
static void __msgpack_pack_string( msgpack_packer *pk, const void *string, size_t n );
static int convertJsonToMsgPack(char *data, char **encodedData);
static void decodeMsgpackData(char *encodedData, int encodedDataLen);
static char *convertMsgpackToBlob(char *data, int size);
static char *decodeBlobData(char *data);
/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

void convertJsonToBlob(char *data)
{
	char *encodedData = NULL, *blobData = NULL, *decodedBlob = NULL;
	int encodedDataLen = 0;
	printf("********* Converting json to msgpack *******\n");
	encodedDataLen = convertJsonToMsgPack(data, &encodedData);
	if(encodedDataLen > 0)
	{
		printf("Converting Json data to msgpack is success\n");
		printf("Converted msgpack data is \n%s\n",encodedData);
		decodeMsgpackData(encodedData, encodedDataLen);
	}
	printf("********* Converting msgpack to blob *******\n");
	blobData = convertMsgpackToBlob(encodedData, encodedDataLen);
	if(blobData)
	{
		printf("Json is converted to blob\n");
		printf("blob data is \n%s\n",blobData);
		decodedBlob = decodeBlobData(blobData);
		printf("Decoded blob data is \n%s\n",decodedBlob);
	}
	if(strcmp(encodedData, decodedBlob) == 0)
	{
		printf("Encoded msgpack data and decoded blob data are equal\n");
	}
}

/*----------------------------------------------------------------------------*/
/*                             Internal Functions                             */
/*----------------------------------------------------------------------------*/

static int getItemsCount(cJSON *object)
{
	int count = 0;
	while(object != NULL)
	{
		object = object->next;
		count++;
	}
	return count;
}
static void __msgpack_pack_string( msgpack_packer *pk, const void *string, size_t n )
{
    msgpack_pack_str( pk, n );
    msgpack_pack_str_body( pk, string, n );
}

static void packJsonString( cJSON *item, msgpack_packer *pk )
{
	__msgpack_pack_string(pk, item->string, strlen(item->string));
	__msgpack_pack_string(pk, item->valuestring, strlen(item->valuestring));
}

static void packJsonNumber( cJSON *item, msgpack_packer *pk )
{
	__msgpack_pack_string(pk, item->string, strlen(item->string));
	msgpack_pack_int(pk, item->valueint);
}

static void packJsonBool(cJSON *item, msgpack_packer *pk, bool value)
{
	__msgpack_pack_string(pk, item->string, strlen(item->string));
	if(value)
	{
		msgpack_pack_true(pk);
	}
	else
	{
		msgpack_pack_false(pk);
	}
}
static void packJsonArray(cJSON *item, msgpack_packer *pk)
{
	//printf("*** packing json array ****\n");
	int arraySize = cJSON_GetArraySize(item);
	__msgpack_pack_string(pk, item->string, strlen(item->string));
	msgpack_pack_array( pk, arraySize );
	int i=0;
	for(i=0; i<arraySize; i++)
	{
		cJSON *arrItem = cJSON_GetArrayItem(item, i);
		switch((arrItem->type) & 0XFF)
		{
			case cJSON_True:
				packJsonBool(arrItem, pk, true);
				break;
			case cJSON_False:
				packJsonBool(arrItem, pk, false);
				break;
			case cJSON_String:
				//printf("%s is %s\n",arrItem->string, arrItem->valuestring);
				packJsonString(arrItem, pk);
				break;
			case cJSON_Number:
				//printf("%s is %d\n",arrItem->string, arrItem->valueint);
				packJsonNumber(arrItem, pk);
				break;
			case cJSON_Array:
				packJsonArray(arrItem, pk);
				break;
			case cJSON_Object:
				packJsonObject(arrItem, pk);
				break;
		}
	}
}
static void packJsonObject( cJSON *item, msgpack_packer *pk )
{
	//printf("*** packing json object ****\n");
	if(item->string != NULL)
	{
		//printf("item->string : %s\n",item->string);
		__msgpack_pack_string(pk, item->string, strlen(item->string));
	}
	cJSON *child = item->child;
	msgpack_pack_map( pk, getItemsCount(child));
	//int i = 0;
	while(child != NULL)
	{
		//printf("**** %s item %d ******\n",item->string,++i);
		//printf("%s type is %d \n",child->string, child->type);
		switch((child->type) & 0XFF)
		{
			case cJSON_True:
				packJsonBool(child, pk, true);
				break;
			case cJSON_False:
				packJsonBool(child, pk, false);
				break;
			case cJSON_String:
				//printf("%s is %s\n",child->string, child->valuestring);
				packJsonString(child, pk);
				break;
			case cJSON_Number:
				//printf("%s is %d\n",child->string, child->valueint);
				packJsonNumber(child, pk);
				break;
			case cJSON_Array:
				packJsonArray(child, pk);
				break;
			case cJSON_Object:
				packJsonObject(child, pk);
				break;
		}
		child = child->next;
	}
}

static int convertJsonToMsgPack(char *data, char **encodedData)
{
	cJSON *jsonData=NULL;
	int encodedDataLen = 0;
	jsonData=cJSON_Parse(data);
	if(jsonData != NULL)
	{
		msgpack_sbuffer sbuf;
		msgpack_packer pk;
		msgpack_sbuffer_init( &sbuf );
		msgpack_packer_init( &pk, &sbuf, msgpack_sbuffer_write );
		packJsonObject(jsonData, &pk);
		if( sbuf.data )
		{
		    *encodedData = ( char * ) malloc( sizeof( char ) * sbuf.size );
		    if( NULL != *encodedData )
			{
		        memcpy( *encodedData, sbuf.data, sbuf.size );
			}
			encodedDataLen = sbuf.size;
		}
		msgpack_sbuffer_destroy(&sbuf);
	}
	else
	{
		printf("Failed to parse JSON\n");
	}
	return encodedDataLen;
}

static void decodeMsgpackData(char *encodedData, int encodedDataLen)
{
	/* deserialize the buffer into msgpack_object instance. */
	/* deserialized object is valid during the msgpack_zone instance alive. */
	msgpack_zone mempool;
	msgpack_zone_init(&mempool, 2048);

	msgpack_object deserialized;
	msgpack_unpack(encodedData, encodedDataLen, NULL, &mempool, &deserialized);
	printf("Decoded msgpack data is \n");
	/* print the deserialized object. */
	msgpack_object_print(stdout, deserialized);
	puts("");
	msgpack_zone_destroy(&mempool);	
}

static char *convertMsgpackToBlob(char *data, int size)
{
	char* b64buffer =  NULL;
	int b64bufferSize = b64_get_encoded_buffer_size( size );
	b64buffer = malloc(b64bufferSize + 1);
    if(b64buffer != NULL)
    {
        memset( b64buffer, 0, sizeof( b64bufferSize )+1 );

        b64_encode((uint8_t *)data, size, (uint8_t *)b64buffer);
        b64buffer[b64bufferSize] = '\0' ;
		//printf("blob data is \n%s\n",b64buffer);
	}
	return b64buffer;
}

static char *decodeBlobData(char *data)
{
	int size = 0;
	char *decodedData = NULL;
    size = b64_get_decoded_buffer_size(strlen(data));
    decodedData = (char *) malloc(sizeof(char) * size);
    if(decodedData)
    {
		memset( decodedData, 0, sizeof(char) *  size );
		size = b64_decode( (const uint8_t *)data, strlen(data), (uint8_t *)decodedData );
		//printf("Decoded blob data is \n%s\n",decodedData);
		decodeMsgpackData(decodedData, size);
	}
	return decodedData;
}