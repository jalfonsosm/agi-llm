import sys
import os

def patch_convert_script(file_path):
    print(f"Patching {file_path}...")
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # 1. Add BitNetForCausalLM to supported architectures
        target_string = '@Model.register("LLaMAForCausalLM", "LlamaForCausalLM", "MistralForCausalLM", "MixtralForCausalLM")'
        replacement_string = '@Model.register("LLaMAForCausalLM", "LlamaForCausalLM", "MistralForCausalLM", "MixtralForCausalLM", "BitNetForCausalLM")'
        
        patched_arch = False
        if target_string in content:
            content = content.replace(target_string, replacement_string)
            patched_arch = True
            print("Successfully patched BitNetForCausalLM support.")
        elif "BitNetForCausalLM" in content:
            patched_arch = True
            print("Architecture support already patched.")
        else:
            print("Warning: Target architecture string not found!")

        # 2. Inject map_tensor_name to handle ffn_sub_norm
        class_def = 'class LlamaModel(Model):'
        method_injection = '''
    def map_tensor_name(self, name: str, try_suffixes: Sequence[str] = (".weight", ".bias")) -> str:
        if "ffn_sub_norm" in name:
            # Map model.layers.{bid}.mlp.ffn_sub_norm to blk.{bid}.ffn_norm
            return name.replace("model.layers.", "blk.").replace(".mlp.ffn_sub_norm", ".ffn_norm")
        return super().map_tensor_name(name, try_suffixes)
'''
        
        patched_tensor = False
        if 'if "ffn_sub_norm" in name:' in content:
             patched_tensor = True
             print("map_tensor_name patch for ffn_sub_norm already present.")
        elif class_def in content:
            anchor = '    model_arch = gguf.MODEL_ARCH.LLAMA'
            if anchor in content:
                content = content.replace(anchor, anchor + method_injection)
                patched_tensor = True
                print("Successfully injected map_tensor_name for ffn_sub_norm.")
            else:
                print("Warning: Could not find anchor to inject map_tensor_name!")
        else:
            print("Warning: LlamaModel class definition not found!")
            
        if patched_arch or patched_tensor:
            with open(file_path, 'w') as f:
                f.write(content)
            print("File updated.")
        else:
            print("No changes needed or patch failed.")
            
    except Exception as e:
        print(f"Error patching file: {e}")
        sys.exit(1)

def patch_setup_env(file_path):
    print(f"Patching {file_path}...")
    try:
        with open(file_path, 'r') as f:
            content = f.read()
            
        # Add the GGUF model repo to SUPPORTED_HF_MODELS
        new_entry = '    "microsoft/BitNet-b1.58-2B-4T-gguf": {\n        "model_name": "BitNet-b1.58-2B-4T",\n    },'
        
        if "microsoft/BitNet-b1.58-2B-4T-gguf" in content:
            print("Model repo already supported.")
            return

        # Insert before the last closing brace of SUPPORTED_HF_MODELS
        # We look for the existing entry for the non-GGUF model to insert after it
        anchor = '"microsoft/BitNet-b1.58-2B-4T": {'
        if anchor in content:
            # Find the closing brace for this entry
            # It's safer to just insert at the beginning of the dict or after the opening brace
            dict_start = 'SUPPORTED_HF_MODELS = {'
            if dict_start in content:
                content = content.replace(dict_start, dict_start + '\n' + new_entry)
                with open(file_path, 'w') as f:
                    f.write(content)
                print("Successfully added GGUF model repo support.")
            else:
                print("Error: Could not find SUPPORTED_HF_MODELS dictionary.")
                sys.exit(1)
        else:
            print("Error: Could not find anchor model entry.")
            sys.exit(1)

    except Exception as e:
        print(f"Error patching file: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 patch_bitnet.py <path_to_file>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    filename = os.path.basename(file_path)
    
    if filename == "convert-hf-to-gguf-bitnet.py":
        patch_convert_script(file_path)
    elif filename == "setup_env.py":
        patch_setup_env(file_path)
    else:
        print(f"Unknown file to patch: {filename}")
        sys.exit(1)
