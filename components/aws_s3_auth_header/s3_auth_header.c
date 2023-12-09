#include <time.h>
#include "esp_sntp.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "s3_auth_header.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"

static char *TAG = "S3 OTA";

void on_got_time(struct timeval *tv);
void get_time_from_ntp(char *ntp_address);
void get_sha256_as_string(char *input, char *output);
void get_signature_key(char *key, char *dateStamp, char *regionName, char *serviceName, uint8_t *output);
void create_canonical_request(char *signed_headers, char *amz_date, s3_params_t *s3_params, char canonical_request_digest[65], char *out_payload_hash);
char *urlencode(char *originalText, bool ignore_slashes);

static SemaphoreHandle_t got_time_semaphore;

void http_client_set_aws_header(esp_http_client_handle_t http_client, s3_params_t *s3_params)
{
    char authorization_header[250] = {};
    char amz_date[30] = {};
    char content_hash[65] = {};

    get_s3_headers("pool.ntp.org", s3_params, authorization_header, amz_date, content_hash);

    esp_http_client_set_header(http_client, "x-amz-date", amz_date);
    esp_http_client_set_header(http_client, "Authorization", authorization_header);
    esp_http_client_set_header(http_client, "x-amz-content-sha256", content_hash);
    esp_http_client_set_header(http_client, "host", s3_params->host);
}

void calculate_s3_header(char *amz_date, char *date_stamp, s3_params_t *s3_params, char *authorization_header, char *out_payload_hash)
{
    // See for details http://docs.aws.amazon.com/general/latest/gr/signature-v4-examples.html

    char *signed_headers = "host;x-amz-date";
    char canonical_request_digest[65] = {};
    // step 1 create a canonical request
    // Step 2: Create a hash of the canonical request
    create_canonical_request(signed_headers, amz_date, s3_params, canonical_request_digest, out_payload_hash);

    // Step 3: Create a String to Sign
    char credential_scope[100] = {};
    sprintf(credential_scope, "%s/%s/s3/aws4_request", date_stamp, s3_params->region);

    char *algorithm = "AWS4-HMAC-SHA256";
    char string_to_sign[200] = {};
    sprintf(string_to_sign, "%s\n%s\n%s\n%s", algorithm, amz_date, credential_scope, canonical_request_digest);

    // Step 4: Calculate the signature
    uint8_t signature_key[32] = {};
    get_signature_key(s3_params->secret_key, date_stamp, s3_params->region, "s3", signature_key);

    uint8_t signature[32] = {};
    const mbedtls_md_info_t *mbedtls_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(mbedtls_md_info, signature_key, 32, (uint8_t *)string_to_sign, strlen(string_to_sign), signature);

    char signature_str[150] = {};
    char temp[65] = {};

    for (int i = 0; i < 32; i++)
    {
        sprintf(signature_str, "%s%02x", temp, signature[i]);
        strcpy(temp, signature_str);
    }
    // Step 5: Add the signature to the request
    sprintf(authorization_header, "%s Credential=%s/%s, SignedHeaders=%s, Signature=%s",
            algorithm, s3_params->access_key, credential_scope, signed_headers, signature_str);
}

void create_canonical_request(char *signed_headers, char *amz_date, s3_params_t *s3_params, char canonical_request_digest[65], char *out_payload_hash)
{
    // Step 1: Create a canonical request
    char canonical_headers[200] = {};
    sprintf(canonical_headers, "host:%s\nx-amz-date:%s\n", s3_params->host, amz_date);

    get_sha256_as_string(s3_params->content == NULL ? "" : s3_params->content, out_payload_hash);

    char *canonical_query_string = "";
    char canonical_request[300] = {};
    char *encoded_canonical_uri = urlencode(s3_params->canonical_uri, true);
    sprintf(canonical_request, "%s\n%s\n%s\n%s\n%s\n%s",
            s3_params->method, encoded_canonical_uri, canonical_query_string, canonical_headers, signed_headers, out_payload_hash);
    free(encoded_canonical_uri);
    // step 2 create a hash of the canonical request
    get_sha256_as_string(canonical_request, canonical_request_digest);
}

void get_s3_headers(char *ntp_address, s3_params_t *s3_params, char *out_authorization_header, char *out_amz_date, char *out_payload_hash)
{
    if (ntp_address != NULL && s3_params->should_get_time)
    {
        //"pool.ntp.org"
        // printf("getting time from %s\n", ntp_address);
        get_time_from_ntp(ntp_address);
    }

    time_t now = 0;
    time(&now);
    struct tm *time_info = localtime(&now);
    strftime(out_amz_date, 18, "%Y%m%dT%H%M%SZ", time_info);
    char date_stamp[20];
    strftime(date_stamp, sizeof(date_stamp), "%Y%m%d", time_info);

    calculate_s3_header(out_amz_date, date_stamp, s3_params, out_authorization_header, out_payload_hash);
}

void on_got_time(struct timeval *tv)
{
    xSemaphoreGive(got_time_semaphore);
}

void get_time_from_ntp(char *ntp_address)
{
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_setservername(0, ntp_address);
    sntp_init();
    got_time_semaphore = xSemaphoreCreateBinary();
    sntp_set_time_sync_notification_cb(on_got_time);
    xSemaphoreTake(got_time_semaphore, portMAX_DELAY);
}

void get_sha256_as_string(char *input, char *output)
{
    uint8_t shaOutPut[32] = {};

    mbedtls_sha256((uint8_t *)input, strlen(input), shaOutPut, 0);
    char temp[65] = {};

    for (int i = 0; i < 32; i++)
    {
        sprintf(output, "%s%02x", temp, shaOutPut[i]);
        strcpy(temp, output);
    }
}

void get_signature_key(char *key, char *dateStamp, char *regionName, char *serviceName, uint8_t *output)
{
    char *augment = "AWS4";
    char aug_key[strlen(key) + strlen(augment) + 1];
    sprintf(aug_key, "%s%s", augment, key);
    memset(output, 0, 32);
    const mbedtls_md_info_t *mbedtls_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_hmac(mbedtls_md_info, (uint8_t *)aug_key, strlen(aug_key), (uint8_t *)dateStamp, strlen(dateStamp), output);
    mbedtls_md_hmac(mbedtls_md_info, output, 32, (uint8_t *)regionName, strlen(regionName), output);
    mbedtls_md_hmac(mbedtls_md_info, output, 32, (uint8_t *)serviceName, strlen(serviceName), output);
    char *aws4_request = "aws4_request";
    mbedtls_md_hmac(mbedtls_md_info, output, 32, (uint8_t *)aws4_request, strlen(aws4_request), output);
}

char *urlencode(char *originalText, bool ignore_slashes)
{
    // allocate memory for the worst possible case (all characters need to be encoded)
    char *encodedText = (char *)malloc(sizeof(char) * strlen(originalText) * 3 + 1);

    const char *hex = "0123456789abcdef";

    int pos = 0;
    for (int i = 0; i < strlen(originalText); i++)
    {
        if (('a' <= originalText[i] && originalText[i] <= 'z') ||
            ('A' <= originalText[i] && originalText[i] <= 'Z') ||
            ('0' <= originalText[i] && originalText[i] <= '9') ||
            (originalText[i] == '-' || originalText[i] == '.' || originalText[i] == '_' || originalText[i] == '~') ||
            (ignore_slashes && originalText[i] == '/'))
        {
            encodedText[pos++] = originalText[i];
        }

        else
        {
            encodedText[pos++] = '%';
            encodedText[pos++] = hex[originalText[i] >> 4];
            encodedText[pos++] = hex[originalText[i] & 15];
        }
    }
    encodedText[pos] = '\0';
    return encodedText;
}
