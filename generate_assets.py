#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generador de iconos y logos para Q-POD MINI
Crea:
  - qpod_icon.ico (icono para la ventana)
  - qpod_logo.png (logo para la interfaz)
"""

import sys
from PIL import Image, ImageDraw, ImageFont
import os

def create_icon():
    """Crea icono .ico estilo robot"""
    # Crear imagen con fondo transparente
    size = 256
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Colores (tema oscuro con acentos)
    DARK_BG = "#1e1e1e"
    ACCENT = "#4caf50"      # Verde
    ACCENT_ALT = "#2196f3"   # Azul
    
    # Fondo circular
    margin = 10
    draw.ellipse(
        [margin, margin, size - margin, size - margin],
        fill=ACCENT,
        outline=ACCENT_ALT,
        width=3
    )
    
    # Cuerpo del robot (cuadrado redondeado)
    body_x1, body_y1 = size * 0.3, size * 0.25
    body_x2, body_y2 = size * 0.7, size * 0.7
    draw.rectangle(
        [body_x1, body_y1, body_x2, body_y2],
        fill=ACCENT_ALT,
        outline="white",
        width=2
    )
    
    # Ojos (dos círculos)
    eye_radius = 8
    eye_y = body_y1 + 25
    
    # Ojo izquierdo
    eye_l_x = body_x1 + 30
    draw.ellipse(
        [eye_l_x - eye_radius, eye_y - eye_radius,
         eye_l_x + eye_radius, eye_y + eye_radius],
        fill="black",
        outline="white",
        width=1
    )
    
    # Ojo derecho
    eye_r_x = body_x2 - 30
    draw.ellipse(
        [eye_r_x - eye_radius, eye_y - eye_radius,
         eye_r_x + eye_radius, eye_y + eye_radius],
        fill="black",
        outline="white",
        width=1
    )
    
    # Pupila izquierda
    pupil_r = 4
    draw.ellipse(
        [eye_l_x - pupil_r, eye_y - pupil_r,
         eye_l_x + pupil_r, eye_y + pupil_r],
        fill="white"
    )
    
    # Pupila derecha
    draw.ellipse(
        [eye_r_x - pupil_r, eye_y - pupil_r,
         eye_r_x + pupil_r, eye_y + pupil_r],
        fill="white"
    )
    
    # Sonrisa (arco)
    mouth_y = body_y2 - 20
    mouth_x1, mouth_x2 = body_x1 + 40, body_x2 - 40
    draw.arc(
        [mouth_x1, mouth_y - 15, mouth_x2, mouth_y + 15],
        start=0,
        end=180,
        fill="white",
        width=3
    )
    
    # Antenas (líneas pequeñas)
    antenna_y = body_y1 - 5
    draw.line(
        [body_x1 + 20, antenna_y, body_x1 + 15, antenna_y - 20],
        fill="white",
        width=3
    )
    draw.line(
        [body_x2 - 20, antenna_y, body_x2 - 15, antenna_y - 20],
        fill="white",
        width=3
    )
    
    # Patas (4 líneas cortas en los lados)
    leg_y = body_y2
    leg_length = 20
    
    # Pata izquierda arriba
    draw.line(
        [body_x1 - 5, body_y1 + 40, body_x1 - leg_length, body_y1 + 40],
        fill=ACCENT,
        width=3
    )
    
    # Pata izquierda abajo
    draw.line(
        [body_x1 - 5, leg_y - 40, body_x1 - leg_length, leg_y - 40],
        fill=ACCENT,
        width=3
    )
    
    # Pata derecha arriba
    draw.line(
        [body_x2 + 5, body_y1 + 40, body_x2 + leg_length, body_y1 + 40],
        fill=ACCENT,
        width=3
    )
    
    # Pata derecha abajo
    draw.line(
        [body_x2 + 5, leg_y - 40, body_x2 + leg_length, leg_y - 40],
        fill=ACCENT,
        width=3
    )
    
    # Guardar como ICO
    icon_path = "qpod_icon.ico"
    img.save(icon_path, format='ICO')
    print(f"✓ Icono creado: {icon_path}")
    
    return icon_path

def create_logo():
    """Crea logo PNG para la interfaz"""
    width, height = 300, 100
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Tema
    ACCENT = "#4caf50"      # Verde
    TEXT_COLOR = "#e0e0e0"   # Gris claro
    
    # Pequeño círculo con robot (simplificado)
    circle_size = 60
    circle_x, circle_y = 20, 20
    
    # Círculo fondo
    draw.ellipse(
        [circle_x, circle_y, circle_x + circle_size, circle_y + circle_size],
        fill=ACCENT,
        outline=TEXT_COLOR,
        width=2
    )
    
    # Cuerpo del robot mini
    body_size = 30
    body_x = circle_x + 15
    body_y = circle_y + 10
    draw.rectangle(
        [body_x, body_y, body_x + body_size, body_y + body_size],
        fill="#2196f3",
        outline=TEXT_COLOR,
        width=1
    )
    
    # Ojos mini
    eye_r = 3
    draw.ellipse(
        [body_x + 5, body_y + 8, body_x + 5 + eye_r*2, body_y + 8 + eye_r*2],
        fill="white"
    )
    draw.ellipse(
        [body_x + 20, body_y + 8, body_x + 20 + eye_r*2, body_y + 8 + eye_r*2],
        fill="white"
    )
    
    # Intentar usar fuente, si no está disponible, usar default
    try:
        font_large = ImageFont.truetype("arial.ttf", 36)
        font_small = ImageFont.truetype("arial.ttf", 14)
    except:
        font_large = ImageFont.load_default()
        font_small = ImageFont.load_default()
    
    # Texto "Q-POD MINI"
    text_x = circle_x + circle_size + 20
    text_y = 15
    draw.text((text_x, text_y), "Q-POD MINI", fill=ACCENT, font=font_large)
    
    # Subtítulo
    draw.text((text_x, text_y + 45), "Configurador Portátil", fill=TEXT_COLOR, font=font_small)
    
    # Guardar
    logo_path = "qpod_logo.png"
    img.save(logo_path, format='PNG')
    print(f"✓ Logo creado: {logo_path}")
    
    return logo_path

def main():
    print("=" * 50)
    print("Q-POD MINI - Generador de Recursos Visuales")
    print("=" * 50)
    print()
    
    try:
        print("[1/2] Generando icono...")
        icon = create_icon()
        
        print("[2/2] Generando logo...")
        logo = create_logo()
        
        print()
        print("=" * 50)
        print("✓ ¡Recursos creados exitosamente!")
        print("=" * 50)
        print()
        print("Archivos:")
        print(f"  • {icon}")
        print(f"  • {logo}")
        print()
        print("Próximo paso: Ejecuta la GUI")
        print("  python gui_qpod_config.py")
        
    except ImportError:
        print()
        print("❌ Error: PIL/Pillow no está instalado")
        print()
        print("Instala con:")
        print("  pip install pillow")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
