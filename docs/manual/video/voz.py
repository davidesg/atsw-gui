#!/usr/bin/env python3
"""LA VOZ DEL VIDEO: cada rotulo de los guiones, sintetizado con Piper o Azure.

    python3 docs/manual/video/voz.py DIR_SALIDA guion.txt [guion.txt...]

Para cada «rotulo TEXTO» deja DIR_SALIDA/<sha1 de TEXTO>.wav y su duracion
en milisegundos en <sha1>.ms: es lo que escenas.c busca al grabar (VOZ_DIR)
para esperar a que acabe cada frase. El sha1 es del argumento tal cual esta
en el guion, asi que cambiar un rotulo rehace solo su frase.

Lo que se dice sale del rotulo, leido para el oido: las siglas deletreadas,
«d = 1» como «de igual a uno», los decimales con coma. Si un rotulo necesita
decir otra cosa, el guion lo pone detras de « || ».

DOS MOTORES, y se elige por el entorno:

  Piper (local; pip install piper-tts) y una voz en espanol:
    PIPER=/ruta/a/piper   (si no esta en el PATH)
    VOZ_MODELO=/ruta/a/es_ES-sharvard-medium.onnx

  Azure (las voces neuronales del servicio Speech; manda el texto a Azure):
    VOZ_AZURE=es-ES-ElviraNeural        (o es-ES-AlvaroNeural, ...)
    AZURE_SPEECH_REGION=westeurope       (la region del recurso)
    AZURE_SPEECH_KEY=...                 (la clave; si no esta en el entorno
                                          se lee de ~/.config/atsw/azure.env,
                                          con lineas CLAVE=valor)
  La clave NUNCA va en el repositorio. Con VOZ_AZURE no hace falta Piper.
  VOZ_VELOCIDAD (Azure) ajusta el ritmo: "-5%", "+0%"...
"""

import hashlib
import os
import re
import subprocess
import sys
import urllib.request
import wave
from xml.sax.saxutils import escape

# Las siglas y los nombres, como se pronuncian. Por palabras enteras.
SIGLAS = {
    "ATSW": "a te ese uve doble",
    "IPC": "i pe ce", "ipc": "i pe ce",
    "WTI": "uve doble te i", "wti": "uve doble te i",
    "ACF": "a ce efe", "PACF": "pe a ce efe", "CCF": "ce ce efe",
    "LR": "ele erre",
    "drtran": "de erre tran",
    "fug": "fug",
    "fue_gui": "fue gui",
    "AR": "a erre", "MA": "eme a",
    "MANUAL.md": "el manual",
}

# Los modelos y los operadores. En orden: lo largo antes.
REGLAS = [
    (r"«|»", ""),
    (r"\bAR\((\d)\)", r"a erre \1"),
    (r"\bMA\((\d)\)", r"eme a \1"),
    (r"\bm0(\d)\b", r"eme cero \1"),
    (r"\.pre\b", " punto pre"),
    (r"\bb r s\b", "be erre ese"),
    (r"λ", "lambda"),
    (r"ω₀", "omega cero"),
    (r"ω", "omega"),
    (r"Θ", "theta"),
    (r"δ", "delta"),
    (r"σ", "sigma"),
    (r"±", "más menos"),
    (r"\bD = ", "de estacional igual a "),
    (r"\bd = ", "de igual a "),
    (r"\bb = ", "be igual a "),
    (r"\br = ", "erre igual a "),
    (r"\bs = ", "ese igual a "),
    (r"\bk = ", "ka igual a "),
    (r"\bt = ", "te igual a "),
    (r"\br\b", "erre"),
    (r" = ", " igual a "),
    (r"(\d)\.(\d)", r"\1 coma \2"),
    (r"\s*%", " por ciento"),
    (r"…", ""),
]


# LOS NOMBRES QUE LA VOZ DICE MAL, por palabras enteras y despues de leer el
# rotulo para el oido. Azure recibe la pronunciacion en IPA (<phoneme>); Piper,
# una grafia que la imite. Sin esto, Azure dice «lambda» de modo que se oye
# «la moda», y «Box y Jenkins» con la jota espanola. Box y Jenkins, Treadway,
# Ljung: como en ingles, con los sonidos del espanol. Las frases ya
# sintetizadas no se rehacen solas al cambiar esta tabla (la cache va por el
# texto del rotulo): se borra su .wav. Se comprobo transcribiendo lo
# sintetizado con el reconocedor de Azure: «ˈlambda» seguia sonando «lameda»;
# con la silaba partida, «ˈlam.bða», se oye «lambda».
PRONUNCIA = {
    "lambda":   ("ˈlam.bða", "lambda"),
    "Box":      ("ˈboks", "Bocs"),
    "Jenkins":  ("ˈʝeŋkins", "Yénkins"),
    "Treadway": ("ˈtɾe.ðwei", "Trédguei"),
    "Ljung":    ("ˈʝuŋ", "Yung"),
}


def pronuncia(txt, ssml):
    """El texto ya leido para el oido, con PRONUNCIA: SSML para Azure (y
    entonces todo lo demas va escapado) o la grafia para Piper."""
    trozos = re.split(r"(\b(?:%s)\b)" % "|".join(PRONUNCIA), txt)
    sal = []
    for i, t in enumerate(trozos):
        if i % 2 == 0:
            sal.append(escape(t) if ssml else t)
        elif ssml:
            sal.append('<phoneme alphabet="ipa" ph="%s">%s</phoneme>' % (PRONUNCIA[t][0], t))
        else:
            sal.append(PRONUNCIA[t][1])
    return "".join(sal)


def para_el_oido(txt):
    if " || " in txt:
        return txt.split(" || ", 1)[1]
    for a, b in REGLAS:
        txt = re.sub(a, b, txt)
    for k, v in SIGLAS.items():
        txt = re.sub(r"(?<![\w.])" + re.escape(k) + r"(?![\w])", v, txt)
    return txt


def rotulos(guiones):
    for g in guiones:
        with open(g, encoding="utf-8") as f:
            for l in f:
                l = l.strip()
                if l.startswith("rotulo "):
                    arg = l[len("rotulo "):].strip()
                    if arg and arg != "-":
                        yield arg


def azure_conf():
    """La region y la clave: del entorno o de ~/.config/atsw/azure.env."""
    conf = {}
    f = os.path.expanduser("~/.config/atsw/azure.env")
    if os.path.exists(f):
        with open(f, encoding="utf-8") as h:
            for l in h:
                if "=" in l and not l.lstrip().startswith("#"):
                    k, v = l.split("=", 1)
                    conf[k.strip()] = v.strip().strip('"').strip("'")
    clave = os.environ.get("AZURE_SPEECH_KEY") or conf.get("AZURE_SPEECH_KEY")
    region = os.environ.get("AZURE_SPEECH_REGION") or conf.get("AZURE_SPEECH_REGION")
    if not clave or not region:
        sys.exit("faltan AZURE_SPEECH_KEY y AZURE_SPEECH_REGION (en el entorno "
                 "o en ~/.config/atsw/azure.env)")
    return clave, region


def azure(dice, wav, voz, clave, region):
    """Una frase con el servicio Speech de Azure: SSML -> WAV PCM 24 kHz."""
    vel = os.environ.get("VOZ_VELOCIDAD", "+0%")
    ssml = ('<speak version="1.0" xml:lang="es-ES">'
            '<voice name="%s"><prosody rate="%s">%s</prosody></voice></speak>'
            % (voz, vel, pronuncia(dice, True)))
    req = urllib.request.Request(
        "https://%s.tts.speech.microsoft.com/cognitiveservices/v1" % region,
        data=ssml.encode("utf-8"), method="POST",
        headers={"Ocp-Apim-Subscription-Key": clave,
                 "Content-Type": "application/ssml+xml",
                 "X-Microsoft-OutputFormat": "riff-24khz-16bit-mono-pcm",
                 "User-Agent": "atsw-video"})
    with urllib.request.urlopen(req, timeout=60) as r:
        datos = r.read()
    with open(wav + ".tmp", "wb") as h:
        h.write(datos)
    os.replace(wav + ".tmp", wav)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sal = sys.argv[1]
    os.makedirs(sal, exist_ok=True)
    voz_azure = os.environ.get("VOZ_AZURE")
    piper = os.environ.get("PIPER", "piper")
    modelo = os.environ.get("VOZ_MODELO")
    if voz_azure:
        clave, region = azure_conf()
    elif not modelo:
        sys.exit("falta VOZ_AZURE (Azure) o VOZ_MODELO (la voz de Piper, .onnx)")
    hechos = 0
    for arg in dict.fromkeys(rotulos(sys.argv[2:])):
        sha = hashlib.sha1(arg.encode("utf-8")).hexdigest()
        wav = os.path.join(sal, sha + ".wav")
        if not os.path.exists(wav):
            dice = para_el_oido(arg)
            if voz_azure:
                azure(dice, wav, voz_azure, clave, region)
            else:
                subprocess.run([piper, "-m", modelo, "-f", wav], input=pronuncia(dice, False).encode("utf-8"),
                               check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print("voz:", dice)
            hechos += 1
        with wave.open(wav) as w:
            ms = int(1000 * w.getnframes() / w.getframerate())
        with open(os.path.join(sal, sha + ".ms"), "w") as f:
            f.write("%d\n" % ms)
    print("voz: %d frases nuevas en %s" % (hechos, sal))


if __name__ == "__main__":
    main()
