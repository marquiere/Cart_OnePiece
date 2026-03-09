import customtkinter as ctk

app = ctk.CTk()
tabview = ctk.CTkTabview(app)
tabview.grid()
t1 = tabview.add("T1")
m = ctk.CTkOptionMenu(t1, values=["1", "2"])
m.pack()
app.update()
print("Success!")
