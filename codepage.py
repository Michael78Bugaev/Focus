# Генератор шрифта CP866 в формате C-массива

# Шрифт 8x16 (16 байт на символ)
# В данном примере используется упрощенный шрифт (заполнитель)
# Вы можете заменить его на реальные данные шрифта CP866

# Размер шрифта
FONT_WIDTH = 8
FONT_HEIGHT = 16
NUM_CHARS = 256  # Всего 256 символов в CP866

# Заголовок C-файла
header = """
// Шрифт CP866 (8x16)
// Сгенерировано с помощью Python-скрипта
const uint8_t cp866_font[256 * 16] = {
"""

# Заголовок завершения C-файла
footer = """
};
"""

# Генерация данных шрифта
def generate_font_data():
    font_data = []
    for char in range(NUM_CHARS):
        # Генерация случайного глифа (заполнитель)
        # В реальном шрифте здесь должны быть данные для каждого символа
        glyph = [0] * FONT_HEIGHT
        for y in range(FONT_HEIGHT):
            glyph[y] = y + char  # Пример заполнения (замените на реальные данные)
        font_data.extend(glyph)
    return font_data

# Преобразование данных в C-массив
def font_to_c_array(font_data):
    c_array = ""
    for i in range(0, len(font_data), 16):
        row = font_data[i:i+16]
        hex_values = [f"0x{byte:02X}" for byte in row]
        c_array += "    " + ", ".join(hex_values) + ",\n"
    return c_array

# Основная функция
def main():
    font_data = generate_font_data()
    c_array = font_to_c_array(font_data)
    
    # Сохранение в файл
    with open("cp866_font.c", "w") as f:
        f.write(header)
        f.write(c_array)
        f.write(footer)
    
    print("Файл cp866_font.c успешно создан!")

if __name__ == "__main__":
    main()