# בניית RMX16 VST3 בענן — ללא התקנת כלי פיתוח במחשב

1. צור חשבון GitHub אם אין לך.
2. צור Repository חדש ובחר **Private**.
3. העלה אליו את כל התוכן שבתיקיית `RMX16_Reconstruction` (כולל התיקייה `.github`).
4. באתר GitHub פתח את לשונית **Actions**.
5. בחר **Build RMX16 VST3** ולחץ **Run workflow**.
6. המתן לסיום. אם הריצה ירוקה, פתח אותה וגלול ל־**Artifacts**.
7. הורד את `RMX16-VST3-Windows-x64.zip`, חלץ אותו, והעתק את תיקיית `.vst3` ל־`C:\Program Files\Common Files\VST3`.
8. ב־FL Studio בצע Plugin Manager > Find installed plugins.

הערות:
- אין צורך להתקין Visual Studio, CMake או JUCE במחשב שלך; GitHub Actions מספק סביבת Windows לבנייה.
- נדרש חשבון GitHub וגישה לאינטרנט כדי להעלות את קובצי המקור.
- העלה את הפרויקט כ־Private כדי שלא לפרסם את קוד המקור.
- אם הבנייה נכשלת, פתח את הריצה ב־Actions והעתק את השגיאה האחרונה. הצלחת הבנייה אינה מובטחת עד שה־workflow מסתיים בהצלחה.
