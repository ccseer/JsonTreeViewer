import json
import os
import sys


def generate_json_file(filename, target_size_bytes):
    """
    Generates a JSON file of approximately the target size.
    It uses an array of objects to make it look like 'real' data.
    """
    print(f"Generating {filename} (Target: {target_size_bytes} bytes)...")

    if target_size_bytes < 100:
        # Small file fallback
        with open(filename, "w", encoding="utf-8") as f:
            json.dump({"message": "small file", "size": target_size_bytes}, f)
        return

    # Buffer for writing to disk
    buffer_size = 1024 * 1024  # 1MB buffer

    with open(filename, "w", encoding="utf-8") as f:
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
        # We want to end with ']'
        # Remaining space: target_size_bytes - current_size - 1 (for ']')
        remaining = target_size_bytes - current_size - 1

        if remaining > 20:  # If we have enough space for a final object
            if count > 0:
                f.write(",")
                remaining -= 1
            # Create a padding object
            # {"padding": "..."}
            padding_overhead = 15  # '{"padding": ""}' is about 15 chars
            if remaining > padding_overhead:
                padding_str = "x" * (remaining - padding_overhead)
                final_obj = {"padding": padding_str}
                s_final = json.dumps(final_obj)
                # Adjust for potential escaped characters if any (though 'x' won't escape)
                actual_final_len = len(s_final.encode("utf-8"))
                if actual_final_len != remaining:
                    # Fine-tune
                    diff = actual_final_len - remaining
                    final_obj["padding"] = "x" * (len(padding_str) - diff)
                    s_final = json.dumps(final_obj)
                f.write(s_final)
            else:
                # Just write a small object if space is tight
                f.write(json.dumps({"end": True}))

        f.write("]")

    actual_size = os.path.getsize(filename)
    print(
        f"\rDone: {filename} - Actual: {actual_size} bytes (Target: {target_size_bytes})"
    )


if __name__ == "__main__":
    # 1k = 1024
    # 11M = 11 * 1024 * 1024
    # 101M = 101 * 1024 * 1024
    # 1.01G = 1.01 * 1024 * 1024 * 1024

    configs = [
        ("1k.json", 1024),
        ("11M.json", 11 * 1024 * 1024),
        ("101M.json", 101 * 1024 * 1024),
        ("1.01G.json", int(1.01 * 1024 * 1024 * 1024)),
    ]

    for filename, size in configs:
        generate_json_file(filename, size)
