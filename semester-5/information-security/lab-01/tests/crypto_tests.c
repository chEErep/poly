#include <stdio.h>
#include <string.h>

#include "crypto.h"

// Печатаем место ошибки и освобождаем оба криптографических контекста
static int test_failed(const char *step,
                       lab_crypto_context *server,
                       lab_crypto_context *client)
{
    DWORD error = GetLastError();

    fprintf(stderr, "%s failed, Win32 error: %lu\n",
            step, (unsigned long)error);

    lab_crypto_close(client);
    lab_crypto_close(server);
    return 1;
}

int main(void)
{
    lab_crypto_context server = {0};
    lab_crypto_context client = {0};

    BYTE public_blob[512];
    DWORD public_length = sizeof(public_blob);

    BYTE wrapped_blob[512];
    DWORD wrapped_length = sizeof(wrapped_blob);
    BYTE encrypted[64];
    BYTE decrypted[64];
    DWORD encrypted_length = 0;
    DWORD decrypted_length = 0;

    // Клиент создаёт RSA-пару и передаёт её публичную часть серверу
    if (!lab_crypto_open(&client)) {
        return test_failed("client: open", &server, &client);
    }
    if (!lab_crypto_generate_exchange_key(&client)) {
        return test_failed("client: RSA key", &server, &client);
    }
    if (!lab_crypto_export_public_key(&client,
                                      public_blob,
                                      &public_length)) {
        return test_failed("client: export public key", &server, &client);
                                      }
    if (public_length < 20 || public_blob[0] != PUBLICKEYBLOB) {
        SetLastError(ERROR_INVALID_DATA);
        return test_failed("client: public BLOB format", &server, &client);
    }

    // Сервер создаёт AES-ключ и упаковывает его публичным ключом клиента
    if (!lab_crypto_open(&server)) {
        return test_failed("server: open", &server, &client);
    }
    if (!lab_crypto_create_session_key(&server,
                                       public_blob,
                                       public_length,
                                       wrapped_blob,
                                       &wrapped_length)) {
        return test_failed("server: create session key", &server, &client);
                                       }

    // Клиент раскрывает AES-ключ своим приватным RSA-ключом
    if (!lab_crypto_accept_session_key(&client,
                                       wrapped_blob,
                                       wrapped_length)) {
        return test_failed("client: accept session key", &server, &client);
                                       }

    // Клиент шифрует, сервер расшифровывает
    if (!lab_crypto_encrypt_message(&client,
                                    (const BYTE *)"hello", 5,
                                    encrypted, sizeof(encrypted),
                                    &encrypted_length)) {
        return test_failed("client: encrypt message", &server, &client);
                                    }

    if (!lab_crypto_decrypt_message(&server,
                                    encrypted, encrypted_length,
                                    decrypted, sizeof(decrypted),
                                    &decrypted_length)) {
        return test_failed("server: decrypt message", &server, &client);
                                    }

    if (decrypted_length != 5 || memcmp(decrypted, "hello", 5) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return test_failed("compare hello", &server, &client);
    }

    // Проверяем и обратное направление: сервер → клиент
    if (!lab_crypto_encrypt_message(&server,
                                    (const BYTE *)"world", 5,
                                    encrypted, sizeof(encrypted),
                                    &encrypted_length)) {
        return test_failed("server: encrypt message", &server, &client);
                                    }

    if (!lab_crypto_decrypt_message(&client,
                                    encrypted, encrypted_length,
                                    decrypted, sizeof(decrypted),
                                    &decrypted_length)) {
        return test_failed("client: decrypt message", &server, &client);
                                    }

    if (decrypted_length != 5 || memcmp(decrypted, "world", 5) != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return test_failed("compare world", &server, &client);
    }

    printf("Crypto test passed: public BLOB %lu bytes, "
           "wrapped BLOB %lu bytes, AES round-trip OK.\n",
           (unsigned long)public_length,
           (unsigned long)wrapped_length);

    lab_crypto_close(&client);
    lab_crypto_close(&server);
    return 0;
}
