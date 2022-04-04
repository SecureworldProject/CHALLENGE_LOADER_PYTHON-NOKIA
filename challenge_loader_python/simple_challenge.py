import psutil

props_dict={} 

def init(props):
    global props_dict
    print("Python: Enter in init")#+ challenge.parameters)
    #params es un diccionario
    props_dict= props
    
    executeChallenge()
    return 0



def executeChallenge():
    print("Starting execute")
    cad=""
    for i in range(props_dict["param2"]):
        cad+=props_dict["param1"]

    result = bytes(cad,'utf-8')
    print(result)
    return result

if __name__ == "__main__":
    midict={"param1": "hola", "param2":3}
    init(midict)
    executeChallenge()
