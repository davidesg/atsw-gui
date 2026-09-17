import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# --- Datos ---
data = pd.read_csv(
    "IPC.volexp",
    sep=r"\s+",
    header=0,
    names=["t", "var1", "var2", "var3", "cov12", "cov13", "cov23"],
)

# t=1 -> febrero 2002, t=215 -> diciembre 2019
start_date = pd.Timestamp("2002-02-01")
data["fecha"] = pd.date_range(start=start_date, periods=len(data), freq="MS")

# Filtrar desde t=50
data = data[data["t"] >= 50].copy()

# --- DT anualizadas (en mismas unidades que datos: puntos porcentuales) ---
# Var mensual en (100·log)^2 -> DT anual = sqrt(12 * var)
data["dt1"] = np.sqrt(12 * data["var1"])
data["dt2"] = np.sqrt(12 * data["var2"])
data["dt3"] = np.sqrt(12 * data["var3"])

# --- Correlaciones ---
data["rho12"] = data["cov12"] / np.sqrt(data["var1"] * data["var2"])
data["rho13"] = data["cov13"] / np.sqrt(data["var1"] * data["var3"])
data["rho23"] = data["cov23"] / np.sqrt(data["var2"] * data["var3"])

# --- Gráfico ---
fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
fig.suptitle("Volatilidad condicional — IPC (t ≥ 50, 2006:03 – 2019:12)", fontsize=13)

colores = ["#1f77b4", "#ff7f0e", "#2ca02c"]
fechas = data["fecha"]

# Panel 1: DT anualizadas
ax1 = axes[0]
ax1.plot(fechas, data["dt1"], color=colores[0], lw=1.2, label="Serie 1")
ax1.plot(fechas, data["dt2"], color=colores[1], lw=1.2, label="Serie 2")
ax1.plot(fechas, data["dt3"], color=colores[2], lw=1.2, label="Serie 3")
ax1.set_ylabel("DT anualizada\n(puntos porcentuales)", fontsize=10)
ax1.legend(fontsize=9)
ax1.grid(True, alpha=0.3)
ax1.set_title("Desviaciones típicas anualizadas  [√(12·σ²ₜ)]", fontsize=10)

# Panel 2: Correlaciones
ax2 = axes[1]
ax2.plot(fechas, data["rho12"], color=colores[0], lw=1.2, label="ρ(1,2)")
ax2.plot(fechas, data["rho13"], color=colores[1], lw=1.2, label="ρ(1,3)")
ax2.plot(fechas, data["rho23"], color=colores[2], lw=1.2, label="ρ(2,3)")
ax2.axhline(0, color="black", lw=0.8, ls="--")
ax2.set_ylabel("Correlación condicional", fontsize=10)
ax2.set_ylim(-1.05, 1.05)
ax2.legend(fontsize=9)
ax2.grid(True, alpha=0.3)
ax2.set_title("Correlaciones condicionales  [σ_{ij,t} / (σ_{i,t}·σ_{j,t})]", fontsize=10)

# Eje X
ax2.xaxis.set_major_locator(mdates.YearLocator(2))
ax2.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
plt.xticks(rotation=30)

plt.tight_layout()
plt.savefig("IPC_volatilidad.pdf", dpi=150, bbox_inches="tight")
plt.savefig("IPC_volatilidad.png", dpi=150, bbox_inches="tight")
print("Gráfico guardado: IPC_volatilidad.pdf y IPC_volatilidad.png")
