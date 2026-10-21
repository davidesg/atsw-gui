"""atsw-mt: el servidor de la clase MT (docs/ESTUDIO-terminal-mcp.md §2.1).

Fase 0: arranca, dice su version y donde estan el proyecto y los motores.
Las instrucciones de verdad, las de mtram recortadas a menos de 20 KB (§3.3),
llegan con la fase 1b.
"""

from atsw_mcp import comun

SERVIDOR = "atsw-mt"
INSTRUCCIONES = (
    "Servidor atsw-mt de ATSW, clase MT (Transferencia). "
    "Fase 0: solo tiene las herramientas version y estado; "
    "todavia no analiza ni estima."
)


def crea():
    return comun.crea_servidor(SERVIDOR, "mt", "MT", INSTRUCCIONES)


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
