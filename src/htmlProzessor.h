/*
DALY2MQTT Project
https://github.com/softwarecrash/DALY2MQTT
*/
String htmlProcessor(const String &var)
{
    if (var == F("pre_head_template"))
        return (FPSTR(HTML_HEAD));
    if (var == F("pre_foot_template"))
        return (FPSTR(HTML_FOOT));
    if (var == F("pre_swversion"))
        return (SWVERSION);   
    return String();
}
