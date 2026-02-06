import re
from pathlib import Path

# Best-effort German -> English translator for VW PQ CAN signal names.
#
# Important constraints:
# - Only the FIRST quoted string on each line is translated (signal name).
# - Unit strings (like "km/h") and other quoted fields are left untouched.
#
# This file is a semi-structured, DBC-derived export; many identifiers are:
# - Truncated (e.g. "Kupplungssteifigkei")
# - Abbreviated (e.g. "Sta", "Sign", "Ueberw")
# - Concatenated without underscores
#
# So the approach is:
# 1) Fix known bad artifacts from previous substring replacements
# 2) Apply phrase replacements (longest first)
# 3) Token-aware replacements for underscore-separated tokens
# 4) Targeted substring replacements inside remaining tokens


# Fixes for artifacts we know can appear from earlier passes.
FIXUPS = [
    ("OilTemperaturee", "OilTemperature"),
    ("AmbientTemperaturee", "AmbientTemperature"),
    ("Kuehlcenterl", "Kuehlmittel"),  # undo accidental "mitte" replacement inside "mittel"
    ("ClutchStiffness_Hinten__Is", "ClutchStiffness_rear__Actual"),
    ("BR5_Druckvalid", "BR5_Pressure_valid"),
]


# Phrase replacements (apply to full name string).
PHRASE_MAP: list[tuple[str, str]] = [
    ("Verbauinformation_gueltig", "InstallInfo_valid"),
    ("CAN_Infotainment_verbaut", "CAN_Infotainment_installed"),
    ("Batteriespannung_Bordnetzbatter", "OnboardBattery_Voltage"),
    ("Angezeigte_Geschwindigkeit", "Indicated_Speed"),
    ("Motordrehzahlgradient", "EngineRPM_Gradient"),
    ("Vorzeichen_Motordrehzahl", "EngineRPM_Gradient_Sign"),
    ("Geschwindigkeitsbegrenzung_aktiv", "SpeedLimiter_active"),
    ("Geschwindigkeitsbegrenzung", "SpeedLimiter"),
    ("Geschwindigkeitsbegren", "SpeedLimiter"),
    ("Geschwindikeitsbegren", "SpeedLimiter"),
    ("Geschwindigkegrenzung", "SpeedLimiter"),
    ("Kupplungsschalter", "ClutchSwitch"),
    ("Kickdownschalter", "KickdownSwitch"),
    ("Lenkradheizung", "SteeringWheelHeater"),
    ("Kupplung_komplett_offen", "Clutch_fully_open"),
    ("Leerlaufsolldrehzahl", "IdleTargetRPM"),
    ("Leerlauf_Solldrehzahl", "IdleTargetRPM"),
]


# Exact token translations for underscore-separated tokens.
TOKEN_MAP: dict[str, str] = {
    # Directions / positions (only when it's a whole token)
    "vorn": "front",
    "hinten": "rear",
    "links": "left",
    "rechts": "right",
    "mitte": "center",
    "VL": "FrontLeft",
    "VR": "FrontRight",
    "HL": "RearLeft",
    "HR": "RearRight",
    # Common words
    "Fahrzeug": "Vehicle",
    "Motor": "Engine",
    "Getriebe": "Transmission",
    "Kupplung": "Clutch",
    "Bremse": "Brake",
    "Lenkung": "Steering",
    "Lenkwinkel": "SteeringAngle",
    "Lenksaeule": "SteeringColumn",
    "Lenkhilfe": "PowerSteering",
    "Geschwindigkeit": "Speed",
    "Drehzahl": "RPM",
    "Motordrehzahl": "EngineRPM",
    "Motorleistung": "EnginePower",
    "Drehmoment": "Torque",
    "Max": "Max",
    "Min": "Min",
    "Soll": "Target",
    "Ist": "Actual",
    "Wunsch": "Desired",
    "Wert": "Value",
    "Vorgabewert": "Setpoint",
    "Einheit": "Unit",
    "Unit": "Unit",
    "Status": "Status",
    "Fehler": "Error",
    "Fehlerstatus": "ErrorStatus",
    "Fehlerlampe": "ErrorLamp",
    "Warnlampe": "WarningLamp",
    "Warnung": "Warning",
    "Anforderung": "Request",
    "Unterdrueckung": "Suppress",
    "valid": "valid",
    "gueltig": "valid",
    "gefiltert": "filtered",
    "aktiv": "active",
    "ausgeschaltet": "disabled",
    # Temperatures / pressures
    "Temperatur": "Temperature",
    "Aussentemperatur": "AmbientTemperature",
    "Oeltemperatur": "OilTemperature",
    "Oltemperatur": "OilTemperature",
    "Oeldruck": "OilPressure",
    "Druck": "Pressure",
    "Ladedruck": "BoostPressure",
    # Other systems
    "Allrad": "AWD",
    "Kuehlmittel": "Coolant",
    "Kuehlung": "Cooling",
    "Schalter": "Switch",
    "Schluessel": "Key",
    "Zaehler": "Counter",
    "Zahler": "Counter",
    "Zeit": "Time",
    "Zeitstempel": "Timestamp",
    "Standzeit": "StandTime",
    "Kilometerstand": "Odometer",
}

# Case-insensitive lookup table for tokens.
TOKEN_MAP_CASEFOLD: dict[str, str] = {k.casefold(): v for k, v in TOKEN_MAP.items()}


# Targeted substring replacements inside tokens (ordered longest-first).
SUBSTRING_MAP: list[tuple[str, str]] = [
    ("Kupplungssteifigkei", "ClutchStiffness"),
    ("Kupplungssteifigke", "ClutchStiffness"),
    ("Kupplungssteifig", "ClutchStiffness"),
    ("Kupplungs", "Clutch"),
    ("Sportschalter", "SportSwitch"),
    ("Sollbeschleunigung", "TargetAcceleration"),
    ("Istbeschl", "ActualAccel"),
    ("Solldz", "TargetRPM"),
    ("RPManhebung", "RPMIncrease"),
    ("Abbrechen", "Cancel"),
    ("Bewertungsfaktor", "RatingFactor"),
    ("Verschleissindex", "WearIndex"),
    ("Verschleissind", "WearIndex"),
    ("Russindex", "SootIndex"),
    ("Niveauregulie", "LevelControl"),
    ("LeistDichte", "PowerDensity"),
    ("ULeistung", "Power"),
    ("Leistung", "Power"),
    ("Lenkeingriff", "SteeringIntervention"),
    ("aktLenkeingriff", "activeSteeringIntervention"),
    ("bremst", "braking"),
    ("StaDruckschw", "Status_PressureThreshold"),
    ("Druckschw", "PressureThreshold"),
    ("Druckvalid", "Pressure_valid"),
    ("Sign_Druck", "Signal_Pressure"),
    ("Sta_Druck", "Status_Pressure"),
    ("Bremsdruck", "BrakePressure"),
    ("Bremslicht", "BrakeLight"),
    ("Notbremsung", "EmergencyBraking"),
    ("Brems", "Brake"),
    ("Querbeschleunigung", "LateralAcceleration"),
    ("gemessene", "measured"),
    ("Vorzeichen", "Sign"),
    ("Uebertemp", "Overtemp"),
    ("Uebersp", "Overvoltage"),
    ("Ubertemperatur", "Overtemperature"),
    ("Ubertemp", "Overtemp"),
    ("Ueberw", "Monitor"),
    ("Ueber", "Over"),
    ("Unterdrueckung", "Suppress"),
    ("Warnungen", "Warnings"),
    ("Warnung", "Warning"),
    ("Leuchtweitenregulierung", "HeadlightLeveling"),
    ("Waehlhebelausleuchtung", "ShifterIllumination"),
    ("Wischwasserheizung", "WasherHeater"),
    ("Sitzbelueftung", "SeatVentilation"),
    ("abschalten", "disable"),
    ("Helligkeit", "Brightness"),
    ("Fernlicht", "HighBeam"),
    ("Abblendlicht", "LowBeam"),
    ("Lichtschalt", "LightSwitch"),
    ("Lichtsensor", "LightSensor"),
    ("Blinker", "TurnSignal"),
    ("Wischer", "Wiper"),
    ("Kuehlm", "Coolant"),
    ("Kuehlmittel", "Coolant"),
    ("Kuehl", "Cool"),
    ("kuehl", "cool"),
    ("Feld", "Field"),
    ("ltemperaturschutz", "OilTempProtection"),
    ("verbaut", "installed"),
    ("Verbau", "Install"),
    ("Kombi", "Cluster"),
    ("Bordnetz", "OnboardPower"),
    ("Klima", "HVAC"),
    ("Dieselpumpe", "DieselPump"),
    ("Tankwarnung", "FuelWarning"),
    ("Heissleuchten", "OverheatLamp"),
    ("Vorwarnung", "PreWarning"),
    ("Dynamische", "Dynamic"),
    ("Ruckmeldung", "Feedback"),
    ("Momenten", "Torque"),
    ("Moment", "Torque"),
    ("ungenau", "inaccurate"),
    ("Gradientenbegrenzung", "GradientLimit"),
    ("Zwischengas", "RevMatch"),
    ("Schubabschaltunterstuetzung", "FuelCutoffAssist"),
    ("Heizung", "Heater"),
    ("Wegimpulszaehlerstatus", "DistancePulseCounterStatus"),
    ("Wegimpulszaehler", "DistancePulseCounter"),
    ("Wegimpuls", "DistancePulse"),
    ("zaehler", "counter"),
    ("Zustand_der", "State_of"),
    ("km_Stand", "Odometer_km"),
    ("km_", "km_"),
]


FIRST_QUOTED_RE = re.compile(r'\"([^\"\\]*(?:\\.[^\"\\]*)*)\"')


def _apply_all(text: str, replacements: list[tuple[str, str]]) -> str:
    out = text
    for src, dst in replacements:
        out = out.replace(src, dst)
    return out


def translate_name(name: str) -> str:
    out = name

    out = _apply_all(out, FIXUPS)
    out = _apply_all(out, PHRASE_MAP)

    # Token-aware pass: only translate whole underscore-separated tokens.
    parts = out.split("_")
    for i, token in enumerate(parts):
        direct = TOKEN_MAP.get(token)
        if direct is not None:
            parts[i] = direct
            continue

        folded = TOKEN_MAP_CASEFOLD.get(token.casefold())
        if folded is not None:
            parts[i] = folded
    out = "_".join(parts)

    # Targeted substring pass for concatenated/truncated tokens.
    out = _apply_all(out, SUBSTRING_MAP)

    # Small cleanup
    out = out.replace("__", "__")
    return out


def translate_file(path: Path) -> tuple[int, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines(keepends=True)

    changed_lines = 0
    changed_names = 0

    new_lines: list[str] = []
    for line in lines:
        m = FIRST_QUOTED_RE.search(line)
        if not m:
            new_lines.append(line)
            continue

        original = m.group(1)
        translated = translate_name(original)
        if translated != original:
            # Replace only the first quoted string occurrence
            start, end = m.span(1)
            line = line[:start] + translated + line[end:]
            changed_lines += 1
            changed_names += 1

        new_lines.append(line)

    new_text = "".join(new_lines)
    if new_text != text:
        path.write_text(new_text, encoding="utf-8", newline="")

    return changed_lines, changed_names


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    target = repo_root / "src" / "functions" / "canview" / "vw_pq_chassis_dbc - Copy.txt"
    backup = target.with_suffix(target.suffix + ".bak")

    if not target.exists():
        print(f"ERROR: File not found: {target}")
        return 2

    if not backup.exists():
        backup.write_bytes(target.read_bytes())
        print(f"Backup created: {backup}")
    else:
        print(f"Backup already exists: {backup}")

    changed_lines, changed_names = translate_file(target)
    print(f"Translated names: {changed_names} (on {changed_lines} lines)")
    print(f"Done: {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
