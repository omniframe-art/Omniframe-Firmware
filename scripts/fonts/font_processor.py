import freetype
import numpy as np
from PIL import Image, ImageEnhance
import os

# Create output directories
os.makedirs("font_c_arrays", exist_ok=True)
os.makedirs("font_bitmaps", exist_ok=True)

# Load the font
letter_height = 20
face = freetype.Face("LibreBaskerville-Medium.ttf")
face.set_char_size(letter_height * 64)  # 30px height

# Characters to render
characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:,!?/"

# Function to generate C array from image
def generate_c_array(image_array, char_name, output_file):
    # Ensure the image has exactly 20 rows
    image_array = np.array(image_array)[:letter_height]
    
    # Calculate the width in bytes (ceil(width/2))
    width_in_bytes = (len(image_array[0]) + 1) // 2

    compressed = []
    for row in image_array:
        output_row = []
        # Process pairs of pixels
        for i in range(0, len(row), 2):
            first_pixel = row[i] & 0x0F
            second_pixel = row[i + 1] & 0x0F if i + 1 < len(row) else 0

            byte = (second_pixel << 4) | first_pixel
            output_row.append(byte)
        
        # Ensure each row has exactly width_in_bytes
        output_row = output_row[:width_in_bytes]
        while len(output_row) < width_in_bytes:
            output_row.append(0xFF)
            
        compressed.extend(output_row)

    # Write as a C array with exact dimensions
    with open(output_file, "a") as c_file:
        c_file.write(f"static const uint8_t {char_name}[LETTER_SIZE][{width_in_bytes}] = {{\n")
        
        # Output exactly letter_height rows
        for i in range(0, len(compressed), width_in_bytes):
            if i // width_in_bytes >= letter_height:
                break
            c_file.write("  {")
            c_file.write(", ".join(f"0x{byte:02X}" for byte in compressed[i:i+width_in_bytes]))
            c_file.write("},\n")

        c_file.write("};\n\n")

# Store processed images
processed_images = {}

# Phase 1: Generate all bitmaps
print("Generating bitmaps...")
for char in characters:
    # Load and render the character
    face.load_char(char)
    bitmap = face.glyph.bitmap

    # Get bitmap data
    width, height = bitmap.width, bitmap.rows
    buffer = np.array(bitmap.buffer, dtype=np.uint8).reshape(height, width)

    # Resize to have a height of 30 while maintaining aspect ratio
    new_height = letter_height
    new_width = int((width / height) * new_height) if height > 0 else 1

    image = Image.fromarray(buffer)
    image = image.resize((new_width, new_height), Image.LANCZOS)

    # Ensure the new image has exact dimensions
    new_width = ((new_width + 1) // 2) * 2  # Make width even
    new_image = Image.new("L", (new_width, letter_height))  # Exact height
    new_image.paste(image, (0, 0))

    # Increase the contrast by 1.2
    enhancer = ImageEnhance.Contrast(new_image)
    new_image = enhancer.enhance(3)

    new_image = new_image.quantize(colors=16)

    # Save the image
    new_image.save(f"font_bitmaps/{char}.bmp", format="BMP")
    
    # Store for C array generation
    processed_images[char] = new_image

print(f"Bitmaps saved to font_bitmaps/")

# Phase 2: Generate C arrays
output_c_file = "font_c_arrays/font_bitmaps.h"

# Clear existing file
with open(output_c_file, "w") as f:
    f.write("// Auto-generated font data\n\n")

print("Generating C arrays...")
for char in characters:
    # Convert digit characters to words, keep letters as is
    char_name = {
        '0': 'ZERO', '1': 'ONE', '2': 'TWO', '3': 'THREE', '4': 'FOUR',
        '5': 'FIVE', '6': 'SIX', '7': 'SEVEN', '8': 'EIGHT', '9': 'NINE'
    }.get(char, char)
    
    generate_c_array(processed_images[char], char_name, output_c_file)

print(f"C arrays written to {output_c_file}")
