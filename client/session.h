#pragma once

/*
 * Подключается к серверу по IP, авторизуется под именем username
 * и ведёт интерактивную сессию чата до /quit или обрыва соединения.
 * fallback_name — имя сервера, отображаемое если SERVER: не пришёл.
 */
int run_session(const char *ip,
                const char *username);
