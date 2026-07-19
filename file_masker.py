import os
import shutil
import tkinter as tk
from tkinter import messagebox, scrolledtext

class FileMaskerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Code Export Helper")
        self.root.geometry("700x500")

        self.label = tk.Label(root, text="文件后缀处理工具 (File Extension Helper)", font=("Microsoft YaHei", 14, "bold"))
        self.label.pack(pady=15)

        self.desc_label = tk.Label(root, text="目标文件类型 (Targets): .h, .cpp, .cmake", font=("Microsoft YaHei", 10))
        self.desc_label.pack(pady=5)

        self.btn_frame = tk.Frame(root)
        self.btn_frame.pack(pady=15)

        # Button 1: Copy to .txt
        self.btn_mask = tk.Button(self.btn_frame, text="复制内容到 .txt\n(Copy content to .txt)", 
                                  command=self.mask_files, bg="#e1f5fe", width=25, height=3, font=("Microsoft YaHei", 10))
        self.btn_mask.grid(row=0, column=0, padx=20)

        # Button 2: Remove .txt suffix
        self.btn_restore = tk.Button(self.btn_frame, text="移除 .txt 后缀\n(Remove .txt suffix)", 
                                     command=self.restore_files, bg="#e8f5e9", width=25, height=3, font=("Microsoft YaHei", 10))
        self.btn_restore.grid(row=0, column=1, padx=20)

        self.log_area = scrolledtext.ScrolledText(root, width=80, height=20, font=("Consolas", 9))
        self.log_area.pack(pady=10, padx=10)

        self.target_exts = {'.h', '.cpp', '.cmake'}

    def log(self, message):
        self.log_area.insert(tk.END, message + "\n")
        self.log_area.see(tk.END)
        self.root.update()

    def mask_files(self):
        if not messagebox.askyesno("确认", "确定要将当前目录及子目录下所有的 .h/.cpp/.cmake 文件复制为 .txt 吗？\nAre you sure you want to copy all source files to .txt?"):
            return

        self.log("-" * 40)
        self.log("开始复制文件 (Start Copying)...")
        count = 0
        cwd = os.getcwd()
        self.log(f"工作目录 (Working Directory): {cwd}")
        
        for root_dir, _, files in os.walk(cwd):
            for file in files:
                _, ext = os.path.splitext(file)
                if ext.lower() in self.target_exts:
                    src_path = os.path.join(root_dir, file)
                    dst_path = src_path + ".txt"
                    try:
                        shutil.copy2(src_path, dst_path)
                        self.log(f"[COPY] {file} -> {file}.txt")
                        count += 1
                    except Exception as e:
                        self.log(f"[ERROR] {file}: {e}")
        
        self.log(f"完成! 共处理 {count} 个文件。")
        messagebox.showinfo("完成", f"操作完成！\n共复制了 {count} 个文件。")

    def restore_files(self):
        if not messagebox.askyesno("确认", "确定要将所有的 .h.txt / .cpp.txt / .cmake.txt 文件还原吗？\n这将重命名文件并去掉 .txt 后缀 (会覆盖同名原文件)。\nAre you sure you want to restore files? This will overwrite existing files."):
            return

        self.log("-" * 40)
        self.log("开始还原文件 (Start Restoring)...")
        count = 0
        cwd = os.getcwd()
        
        for root_dir, _, files in os.walk(cwd):
            for file in files:
                if file.lower().endswith(".txt"):
                    base_name = file[:-4] # Remove .txt
                    _, original_ext = os.path.splitext(base_name)
                    if original_ext.lower() in self.target_exts:
                        src_path = os.path.join(root_dir, file)
                        dst_path = os.path.join(root_dir, base_name)
                        try:
                            # If destination exists, remove it first to allow rename
                            if os.path.exists(dst_path):
                                os.remove(dst_path)
                            
                            os.rename(src_path, dst_path)
                            self.log(f"[RESTORE] {file} -> {base_name}")
                            count += 1
                        except Exception as e:
                            self.log(f"[ERROR] {file}: {e}")
                            
        self.log(f"完成! 共还原 {count} 个文件。")
        messagebox.showinfo("完成", f"操作完成！\n共还原了 {count} 个文件。")

if __name__ == "__main__":
    root = tk.Tk()
    # Try to set icon if available, otherwise ignore
    # root.iconbitmap('icon.ico') 
    app = FileMaskerApp(root)
    root.mainloop()
