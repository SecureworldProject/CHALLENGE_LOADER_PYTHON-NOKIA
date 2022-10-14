### GLOBAL VARIABLES ###
props_dict = {}




### FUNCTIONS ###
def init(props):
    global props_dict
    print("Python: starting challenge init()")

    # Save properties in the global variable
    props_dict = props
    
    # Execute the challenge once during the init, so key is calculated from the beginning
    executeChallenge()
    return 0


def executeChallenge():
    print("Python: starting executeChallenge()")

    # The key will be the result of concatenating the string corresponding to the property 'param1' as many times as the property 'param2' indicates
    cad = ""
    for i in range(props_dict["param2"]):
        cad += props_dict["param1"]

    # Get key as UTF-8 and calculate its length
    key = bytes(cad, 'utf-8')
    key_size = len(key)

    # The result is a tuple (key, key_size)
    result = (key, key_size)
    print("Python:", result)

    return result


if __name__ == "__main__":
    # Use a dictionary as example of properties obtained from the json
    props_example_dict = {"param1": "hola", "param2": 3}
    init(props_example_dict)
    executeChallenge()
