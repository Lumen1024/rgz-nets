#pragma once

/*
 * Подключается к серверу по IP, авторизуется под именем username
 * и ведёт интерактивную сессию чата до /quit или обрыва соединения.
 * fallback_name — имя сервера, отображаемое если SERVER: не пришёл.
 */
void session_run(const char *ip,
                 const char *fallback_name,
                 const char *username);
