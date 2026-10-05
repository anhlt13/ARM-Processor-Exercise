//
// Created by Tran Lan Anh on 29.9.2026.
//

#include "network_task.h"
#include "system_state.h"
#include "IPStack.h"

extern "C"
{
    bool run_tls_client_test(const uint8_t *cert,size_t cert_len,const char *server,const char *request,int timeout,char *out_buf,size_t max_len);
}

static void clearTalkBackCommands(const uint8_t *certificate)
{
    char deleteRequest[512];

    snprintf(deleteRequest,
             sizeof(deleteRequest),
             "DELETE /talkbacks/%d/commands HTTP/1.1\r\n"
             "Host: " TLS_CLIENT_SERVER "\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %u\r\n"
             "Connection: close\r\n"
             "\r\n"
             "api_key=%s",
             TALKBACK_ID,
             (unsigned int)strlen("api_key=" TALLBACK_API_KEY),
             TALLBACK_API_KEY);

    printf("[Network] Clearing old TalkBack commands...\n");

    bool result = run_tls_client_test(certificate,sizeof(THINGSPEAK_CERT),TLS_CLIENT_SERVER,deleteRequest,TLS_CLIENT_TIMEOUT_SECS,NULL,0);

    if (result)
    {
        printf("[Network] Old TalkBack commands cleared\n");
    }
    else
    {
        printf("[Network] Failed to clear TalkBack commands\n");
    }
}

const uint8_t certificate[] = THINGSPEAK_CERT;

void sendToThinkSpeak()
{
    static char body[256];
    float co2 = systemState.measuredCo2;
    float temperature = systemState.temperature;
    float humidity = systemState.relativeHumidity;
    uint16_t fanSpeed = systemState.fanSpeedPercent;
    float co2Setpoint = systemState.co2Setpoint;

    snprintf(body, sizeof(body),"api_key=" API_KEY "&field1=%.1f""&field2=%.1f""&field3=%.1f""&field4=%u""&field5=%.1f",
             co2,
             temperature,
             humidity,
             fanSpeed,
             co2Setpoint);

    static char request[512];
    snprintf(request, sizeof(request),
             "POST /update HTTP/1.1\r\n"
             "Host: " TLS_CLIENT_SERVER "\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %u\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             (unsigned int)strlen(body),
             body);

    //clearTalkBackCommands(certificate);
    bool pass= run_tls_client_test(certificate, sizeof(certificate), TLS_CLIENT_SERVER, request, TLS_CLIENT_TIMEOUT_SECS, NULL, 0);
    if (pass)
    {
        printf("Test passed\n");
    }else
    {
        printf("Test failed\n");
    }
}

void receiveFromThinkSpeak()
{
    static char talkBackRequest[512];
    static char http_response_buf[1024];
    snprintf(talkBackRequest, sizeof(talkBackRequest),"GET /talkbacks/%d/commands/execute?api_key=%s HTTP/1.1\r\n"
                                                        "Host: " TLS_CLIENT_SERVER "\r\n"
                                                        "Connection: close\r\n"
                                                        "\r\n",
                                                        TALKBACK_ID,
                                                        TALLBACK_API_KEY);
    printf("[Network] Requesting TalkBack command...\n");
    bool talkBackReceivePass = run_tls_client_test(certificate, sizeof(certificate), TLS_CLIENT_SERVER, talkBackRequest, TLS_CLIENT_TIMEOUT_SECS, http_response_buf, sizeof(http_response_buf));
        if (talkBackReceivePass)
        {
            printf("TalkBack request completed. Parsing response...\n");
            char *body = strstr(http_response_buf, "\r\n\r\n");

            if (body != NULL)
            {
                body += 4;
                if (strstr(http_response_buf, "Transfer-Encoding: chunked") != NULL)
                {
                    long chunk_size = strtol(body, NULL, 16);

                    if (chunk_size > 0)
                    {
                        char *data_start = strstr(body, "\r\n");
                        if (data_start != NULL)
                        {
                            data_start += 2;
                            float newSetPoint;
                            if (sscanf(data_start, "%f", &newSetPoint) == 1)
                            {
                                printf("[RESPONSE FROM CLOUD] Parsed Command Value: %.1f\n", newSetPoint);
                                if (newSetPoint >= 0.0f && newSetPoint <= 1500.0f) {
                                    if (xQueueSend(cloudSetPointQueue, &newSetPoint, 0) == pdPASS) {
                                        printf("[RESPONSE FROM CLOUD] TalkBack: new CO2 setpoint %.1f ppm sent to Controller\n", newSetPoint);
                                    } else {
                                        printf("TalkBack: setpoint queue is full\n");
                                    }
                                } else {
                                    printf("TalkBack: invalid setpoint %.1f ppm\n", newSetPoint);
                                }
                            }else{
                                printf("TalkBack Error: Failed to parse data from message (Invalid format)!\n");
                            }
                        }
                    }
                    else
                    {
                       // printf("TalkBack Queue is empty (chunk size = 0).\n");
                    }
                }
                else
                {

                    float newSetPoint;
                    if (sscanf(body, "%f", &newSetPoint) == 1)
                    {
                        xQueueSend(cloudSetPointQueue, &newSetPoint, 0);
                        printf(">> Parsed Non-Chunked Value: %.1f\n", newSetPoint);

                    }
                }
            }
        }
        else
        {
            printf("TalkBack request failed\n");
        }
}

void network_task(void *pvParameters)
{
    IPStack ipstack(SSID, PASSWORD);
    ipstack.connect_to_wifi(SSID, PASSWORD);
    while (true)
    {
        if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP)
        {
            sendToThinkSpeak();
            receiveFromThinkSpeak();
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
