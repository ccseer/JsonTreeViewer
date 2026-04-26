import json
import os
import sys

# === CONFIGURATION ===
# All generated test files will be saved in this directory.
# Default is the project root (parent of the 'tests' folder).

# SAVE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAVE_DIR = r"C:\Users\corey\Dev\build_output\JsonTreeViewer"

# =====================


def generate_json_file(filename, target_size_bytes):
    """
    Generates a JSON file of approximately the target size.
    It uses an array of objects to make it look like 'real' data.
    """
    full_path = os.path.join(SAVE_DIR, filename)
    print(f"Generating {full_path} (Target: {target_size_bytes} bytes)...")

    if target_size_bytes < 100:
        # Small file fallback
        with open(full_path, "w", encoding="utf-8") as f:
            json.dump({"message": "small file", "size": target_size_bytes}, f)
        return

    with open(full_path, "w", encoding="utf-8") as f:
        f.write("[")
        current_size = 1  # '['

        count = 0
        # Template object
        template_obj = {
            "index": 0,
            "guid": "5f4dcc3b-5aa2-4c2c-8d1d-04576307127a",
            "isActive": True,
            "balance": "$3,245.12",
            "age": 30,
            "eyeColor": "blue",
            "hairColor": "#ff000f",
            "name": "John Doe",
            "gender": "male",
            "company": "TECHCORP",
            "email": "johndoe@techcorp.com",
            "phone": "+1 (900) 555-1234",
            "address": "123 Main St, Anytown, USA",
            "about": "Excepteur sint occaecat cupidatat non proident.",
            "registered": "2020-01-01T12:00:00 -00:00",
            "latitude": 40.7128,
            "longitude": -74.0060,
            "tags": ["lorem", "ipsum", "dolor", "sit", "amet"],
            "friends": [{"id": 0, "name": "Friend 1"}, {"id": 1, "name": "Friend 2"}],
        }

        template_str = json.dumps(template_obj)
        template_len = len(template_str.encode("utf-8")) + 1  # +1 for comma

        # Safety margin depends on target size
        margin = 50 if target_size_bytes < 2000 else 500

        # Write bulk
        while current_size + template_len < target_size_bytes - margin:
            if count > 0:
                f.write(",")
                current_size += 1

            # Update index to make it slightly different
            template_obj["index"] = count
            s = json.dumps(template_obj)
            f.write(s)
            current_size += len(s.encode("utf-8"))
            count += 1

            if count % 10000 == 0:
                percent = (current_size / target_size_bytes) * 100
                sys.stdout.write(
                    f"\rProgress: {percent:.2f}% ({current_size // (1024*1024)} MB)"
                )
                sys.stdout.flush()

        # Final padding to get close to the exact size
        remaining = target_size_bytes - current_size - 1

        if remaining > 20:
            if count > 0:
                f.write(",")
                remaining -= 1
            padding_overhead = 15
            if remaining > padding_overhead:
                padding_str = "x" * (remaining - padding_overhead)
                final_obj = {"padding": padding_str}
                s_final = json.dumps(final_obj)
                f.write(s_final)
            else:
                f.write(json.dumps({"end": True}))

        f.write("]")

    actual_size = os.path.getsize(full_path)
    print(f"\rDone: {full_path} - Actual: {actual_size} bytes")


def generate_broken_large_json(filename, target_size_mb=120):
    """
    Generates a large JSON file with intentional syntax errors for testing error reporting.
    """
    full_path = os.path.join(SAVE_DIR, filename)
    target_size_bytes = target_size_mb * 1024 * 1024
    print(f"Generating {full_path} (Target: {target_size_mb} MB broken JSON)...")

    with open(full_path, "w", encoding="utf-8") as f:
        f.write('{\n  "status": "success",\n  "data": [\n')
        current_size = f.tell()
        item_id = 0
        while current_size < target_size_bytes - 1024:
            item = f'    {{"id": {item_id}, "name": "item_{item_id}", "payload": "{"x"*60}"}},\n'
            f.write(item)
            item_id += 1
            current_size += len(item)

        # Intentionally corrupt the file near the end
        f.write(f'    {{"id": {item_id}, "error_here": "Invalid escape \\x", \n')
        f.write('    "incomplete_object": {"key": "no_closing_quote\n')
        # File ends here without closing tags


if __name__ == "__main__":
    # Ensure save directory exists
    if not os.path.exists(SAVE_DIR):
        os.makedirs(SAVE_DIR)

    # 1. Base Strategy Test Files
    print("=== Generating Base Strategy Test Files ===")
    base_configs = [
        ("1k.json", 1024),
        ("11M.json", 11 * 1024 * 1024),
        ("101M.json", 101 * 1024 * 1024),
        ("1101M.json", 1101 * 1024 * 1024),
    ]

    for filename, size in base_configs:
        generate_json_file(filename, size)

    # 2. Phase 13: Async Loading Test Files
    print("\n=== Generating Phase 13 Test Files ===")

    # Helper for standard JSON dumps
    def dump_test_file(filename, data, indent=None):
        full_path = os.path.join(SAVE_DIR, filename)
        print(f"Generating {full_path}...")
        with open(full_path, "w") as f:
            json.dump(data, f, indent=indent)

    dump_test_file(
        "test_100k_array.json",
        [{"id": i, "name": f"item_{i}", "value": i * 2} for i in range(100000)],
    )
    dump_test_file(
        "test_100k_object.json",
        {f"key_{i}": {"id": i, "value": i * 2} for i in range(100000)},
    )

    # Nested data
    data_nested = {"level": 0}
    curr = data_nested
    for i in range(1, 100):
        curr["child"] = {"level": i}
        curr = curr["child"]
    dump_test_file("test_nested.json", data_nested, indent=2)

    dump_test_file("test_empty.json", {})

    # 3. Phase 22: Error Reporting Test Files
    print("\n=== Generating Phase 22 Test Files ===")
    generate_broken_large_json("test_broken_large.json", 120)

    print(f"\n=== All test files generated successfully in: {SAVE_DIR} ===")
