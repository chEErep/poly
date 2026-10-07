#include "crypto.h"
#include <string.h>
int lab_crypto_open(lab_crypto_context *context)
{
    if (context == NULL
        || context->provider != 0
        || context->exchange_key != 0 || context->session_key != 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
        }

    return CryptAcquireContextW(&context->provider,
                                NULL,
                                MS_ENH_RSA_AES_PROV_W,
                                PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT) != 0;
}

int lab_crypto_generate_exchange_key(lab_crypto_context *context)
{
    if (context == NULL
        || context->provider == 0
        || context->exchange_key != 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
        }

    return CryptGenKey(context->provider,
                       AT_KEYEXCHANGE,
                       2048u << 16,
                       &context->exchange_key) != 0;
}

int lab_crypto_export_public_key(lab_crypto_context *context,
                                 BYTE *buffer,
                                 DWORD *buffer_length)
{
    if (context == NULL
        || context->exchange_key == 0
        || buffer == NULL
        || buffer_length == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
        }

    return CryptExportKey(context->exchange_key,
                          0,
                          PUBLICKEYBLOB,
                          0,
                          buffer,
                          buffer_length) != 0;
}
int lab_crypto_create_session_key(lab_crypto_context *context,
                                  const BYTE *public_blob,
                                  DWORD public_blob_length,
                                  BYTE *wrapped_blob,
                                  DWORD *wrapped_blob_length)
{
    HCRYPTKEY public_key = 0;
    DWORD error;

    if (context == NULL || context->provider == 0
        || context->session_key != 0
        || public_blob == NULL || public_blob_length == 0
        || wrapped_blob == NULL || wrapped_blob_length == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    // Превращаем полученные байты публичного RSA-ключа в ключ CryptoAPI
    if (!CryptImportKey(context->provider, public_blob,
                        public_blob_length, 0, 0, &public_key)) {
        return 0;
    }

    // Создаём случайный AES-256-ключ. Флаг нужен для его упаковки в BLOB
    if (!CryptGenKey(context->provider, CALG_AES_256,
                     CRYPT_EXPORTABLE, &context->session_key)) {
        error = GetLastError();
        CryptDestroyKey(public_key);
        SetLastError(error);
        return 0;
    }

    // Упаковываем AES-ключ публичным RSA-ключом клиента
    if (!CryptExportKey(context->session_key, public_key,
                        SIMPLEBLOB, CRYPT_OAEP,
                        wrapped_blob, wrapped_blob_length)) {
        error = GetLastError();
        CryptDestroyKey(context->session_key);
        context->session_key = 0;
        CryptDestroyKey(public_key);
        SetLastError(error);
        return 0;
    }

    CryptDestroyKey(public_key);
    return 1;
}

int lab_crypto_accept_session_key(lab_crypto_context *context,
                                  const BYTE *wrapped_blob,
                                  DWORD wrapped_blob_length)
{
    if (context == NULL || context->provider == 0
        || context->exchange_key == 0
        || context->session_key != 0
        || wrapped_blob == NULL || wrapped_blob_length == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    // Приватный RSA-ключ клиента раскрывает упакованный AES-ключ
    return CryptImportKey(context->provider,
                          wrapped_blob,
                          wrapped_blob_length,
                          context->exchange_key,
                          CRYPT_OAEP,
                          &context->session_key) != 0;
}

int lab_crypto_encrypt_message(lab_crypto_context *context,
                               const BYTE *plaintext,
                               DWORD plaintext_length,
                               BYTE *output,
                               DWORD output_capacity,
                               DWORD *output_length)
{
    BYTE iv[LAB_AES_IV_BYTES];
    HCRYPTKEY working_key = 0;
    DWORD encrypted_length;
    DWORD error;

    if (context == NULL || context->provider == 0
        || context->session_key == 0
        || plaintext == NULL || output == NULL
        || output_length == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    *output_length = 0;

    // 16 байт IV + максимум 16 байт дополнения AES-блока
    if (output_capacity < 2u * LAB_AES_IV_BYTES
        || plaintext_length > output_capacity - 2u * LAB_AES_IV_BYTES) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    if (!CryptGenRandom(context->provider, LAB_AES_IV_BYTES, iv)) {
        return 0;
    }

    if (!CryptDuplicateKey(context->session_key, NULL, 0, &working_key)) {
        return 0;
    }

    if (!CryptSetKeyParam(working_key, KP_IV, iv, 0)) {
        error = GetLastError();
        CryptDestroyKey(working_key);
        SetLastError(error);
        return 0;
    }

    // Сначала IV, сразу за ним — исходный текст для шифрования на месте
    memmove(output + LAB_AES_IV_BYTES, plaintext, plaintext_length);
    memcpy(output, iv, LAB_AES_IV_BYTES);
    encrypted_length = plaintext_length;

    if (!CryptEncrypt(working_key, 0, TRUE, 0,
                      output + LAB_AES_IV_BYTES,
                      &encrypted_length,
                      output_capacity - LAB_AES_IV_BYTES)) {
        error = GetLastError();
        CryptDestroyKey(working_key);
        SetLastError(error);
        return 0;
    }

    CryptDestroyKey(working_key);
    *output_length = LAB_AES_IV_BYTES + encrypted_length;
    return 1;
}

int lab_crypto_decrypt_message(lab_crypto_context *context,
                               const BYTE *message,
                               DWORD message_length,
                               BYTE *output,
                               DWORD output_capacity,
                               DWORD *output_length)
{
    HCRYPTKEY working_key = 0;
    DWORD decrypted_length;
    DWORD error;

    if (context == NULL || context->session_key == 0
        || message == NULL || output == NULL
        || output_length == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    *output_length = 0;

    // Должны быть IV и хотя бы один 16-байтный блок шифротекста
    if (message_length < 2u * LAB_AES_IV_BYTES
        || (message_length - LAB_AES_IV_BYTES)
           % LAB_AES_IV_BYTES != 0) {
        SetLastError(ERROR_INVALID_DATA);
        return 0;
    }

    decrypted_length = message_length - LAB_AES_IV_BYTES;
    if (output_capacity < decrypted_length) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
    }

    if (!CryptDuplicateKey(context->session_key, NULL, 0, &working_key)) {
        return 0;
    }

    // Берём IV из начала сообщения, пока ещё ничего не перезаписали
    if (!CryptSetKeyParam(working_key, KP_IV, message, 0)) {
        error = GetLastError();
        CryptDestroyKey(working_key);
        SetLastError(error);
        return 0;
    }

    memmove(output, message + LAB_AES_IV_BYTES, decrypted_length);

    if (!CryptDecrypt(working_key, 0, TRUE, 0,
                      output, &decrypted_length)) {
        error = GetLastError();
        CryptDestroyKey(working_key);
        SetLastError(error);
        return 0;
    }

    CryptDestroyKey(working_key);
    *output_length = decrypted_length;
    return 1;
}
void lab_crypto_close(lab_crypto_context *context)
{
    if (context == NULL) {
        return;
    }

    if (context->session_key != 0) {
        CryptDestroyKey(context->session_key);
        context->session_key = 0;
    }
    if (context->exchange_key != 0) {
        CryptDestroyKey(context->exchange_key);
        context->exchange_key = 0;
    }

    if (context->provider != 0) {
        CryptReleaseContext(context->provider, 0);
        context->provider = 0;
    }
}
