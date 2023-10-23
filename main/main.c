#include <stdio.h>
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "s3_auth_header.h"
#include "protocol_examples_common.h"

static char *TAG = "UPSERT S3";

#define AWS_ACCESS_KEY "MY_IAM_ACCESS_KEY"
#define AWS_ACCESS_SECRET "MY_IAM_ACCESS_SECRET"

esp_err_t on_client_data(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "Length=%d", evt->data_len);
        printf("%.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

void upsert_file()
{
    esp_http_client_config_t esp_http_client_config = {
        .url = "https://esp-read-write.s3.amazonaws.com/test.txt",
        .method = HTTP_METHOD_PUT,
        .event_handler = on_client_data,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&esp_http_client_config);

    char *content = "Hello AWS! 3";
    s3_params_t s3_params = {
        .access_key = MY_IAM_ACCESS_KEY,
        .secret_key = AWS_ACCESS_SECRET,
        .host = "esp-read-write.s3.amazonaws.com",
        .region = "us-east-1",
        .canonical_uri = "/test.txt",
        .content = content,
        .method = "PUT",
        .should_get_time = true};

    http_client_set_aws_header(client, &s3_params);

    esp_http_client_set_post_field(client, content, strlen(content));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void download_file()
{
    esp_http_client_config_t esp_http_client_config = {
        .url = "https://esp-read-write.s3.amazonaws.com/test.txt",
        .method = HTTP_METHOD_GET,
        .event_handler = on_client_data,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&esp_http_client_config);

    char *content = "";
    s3_params_t s3_params = {
        .access_key = MY_IAM_ACCESS_KEY,
        .secret_key = AWS_ACCESS_SECRET,
        .host = "esp-read-write.s3.amazonaws.com",
        .region = "us-east-1",
        .canonical_uri = "/test.txt",
        .content = content,
        .method = "GET",
        .should_get_time = false};

    http_client_set_aws_header(client, &s3_params);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void app_main(void)
{

    nvs_flash_init();
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    upsert_file();
    printf("downloading file\n");
    download_file();
}
