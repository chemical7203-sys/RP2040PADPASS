import os

# Configuration
HTML_SRC_DIR = "rp2040_firmware/src/html"
HTML_FILENAME = "config.html"
C_OUTPUT_DIR = "rp2040_firmware/src/html"
C_OUTPUT_FILENAME = "fsdata.c"

def create_fsdata_c(html_content, filename):
    """
    Converts the content of an HTML file into a C source file
    that represents it as a constant character array.
    This is a simplified version of lwIP's makefsdata tool.
    """
    # Sanitize filename for C variable names
    var_name = filename.replace('.', '_')

    # Start creating the C file content
    c_code = []
    c_code.append('#include "lwip/apps/fs.h"')
    c_code.append('#include "lwip/def.h"')
    c_code.append('\n')

    # Write the content as a C array
    c_code.append(f'const static unsigned char data_{var_name}[] = {{')

    # Add file content as hex bytes
    byte_str = ", ".join(f"0x{byte:02x}" for byte in html_content.encode('utf-8'))
    c_code.append(byte_str)

    c_code.append('};')
    c_code.append('\n')

    # Create the fs_file struct
    c_code.append(f'const struct fsdata_file file_{var_name}[] = {{{{')
    c_code.append(f'  NULL,')
    c_code.append(f'  data_{var_name},')
    c_code.append(f'  "{filename}",')
    c_code.append(f'  {len(html_content)},')
    c_code.append(f'  FS_FILE_FLAGS_HEADER_INCLUDED,')
    c_code.append(f'}}}};')
    c_code.append('\n')

    # Create the root file entry
    c_code.append(f'const struct fsdata_file * const FS_ROOT = file_{var_name};')

    return "\n".join(c_code)

def main():
    """
    Main function to read the HTML and generate the C source file.
    """
    html_path = os.path.join(HTML_SRC_DIR, HTML_FILENAME)
    c_path = os.path.join(C_OUTPUT_DIR, C_OUTPUT_FILENAME)

    print(f"Reading HTML file from: {html_path}")
    try:
        with open(html_path, 'r', encoding='utf-8') as f:
            html_content = f.read()
    except FileNotFoundError:
        print(f"Error: Source file not found at {html_path}")
        return

    print("Generating C source file...")
    c_source_code = create_fsdata_c(html_content, f"/{HTML_FILENAME}")

    print(f"Writing C source file to: {c_path}")
    with open(c_path, 'w', encoding='utf-8') as f:
        f.write(c_source_code)

    print("Done.")

if __name__ == "__main__":
    main()
