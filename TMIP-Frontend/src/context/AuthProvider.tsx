import { createContext, useState } from "react";

export interface AuthContextProps {
    setAuth?: React.Dispatch<React.SetStateAction<AuthObjectType>>;
    auth?: AuthObjectType;
}

export interface AuthObjectType {
    accessToken?: string;
}

const AuthContext = createContext<AuthContextProps>({});

interface Props {
    children: React.ReactNode;
}

export const AuthProvider: React.FC<Props> = ({ children }) => {
    const [auth, setAuth] = useState<AuthObjectType>({});
    return (
        <AuthContext.Provider value={{ auth, setAuth }}>
            {children}
        </AuthContext.Provider>
    )
}

export default AuthContext;