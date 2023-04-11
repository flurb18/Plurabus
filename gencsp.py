CSPDict = {
    "script-src" : "'self' https://www.recaptcha.net/recaptcha/ https://www.gstatic.com/recaptcha/;",
    "img-src" : "'self';",
    "frame-src" : "'self' https://www.recaptcha.net/recaptcha/;",
    "connect-src" : "'self' https://fonts.googleapis.com/ https://fonts.gstatic.com/;",
    "style-src" : "'self' https://fonts.googleapis.com/;",
    "default-src" : "'self' https://fonts.gstatic.com/;",
    "frame-ancestors" : "'self';"
}
WasmCSPDict = CSPDict.copy()
WasmCSPDict["script-src"] = "'unsafe-eval' " + CSPDict["script-src"]

def create_csp(CSP):
    return " ".join("{} {}".format(k,v) for k,v in CSP.items())

DefaultCSP = create_csp(CSPDict)
WasmCSP = create_csp(WasmCSPDict)
