import { forwardRef } from 'react'
import Button from '@mui/material/Button';
import Dialog from '@mui/material/Dialog';
import DialogActions from '@mui/material/DialogActions';
import DialogContent from '@mui/material/DialogContent';
import DialogTitle from '@mui/material/DialogTitle';
import { TransitionProps } from '@mui/material/transitions';
import Zoom from '@mui/material/Zoom';
import { useNavigate } from 'react-router-dom';
import { ReactJSXElement } from '@emotion/react/types/jsx-namespace';
import Paper from '@mui/material/Paper';
import { styled } from '@mui/material/styles';

interface Props {
    open: boolean;
    setOpen: React.Dispatch<React.SetStateAction<boolean>>;
    navLocation?: string;
    title: string;
    message: ReactJSXElement;
}

const Transition = forwardRef(function Transition(
    props: TransitionProps & {
        children: React.ReactElement<any, any>;
    },
    ref: React.Ref<unknown>,
) {
    return <Zoom ref={ref} {...props} />;
});

export const CodeItemRed = styled(Paper)(({ theme }) => ({
    ...theme.typography.body1,
    textAlign: 'left',
    color: 'red',
    fontFamily: 'monospace',
    padding: '10px',
}));

export const CodeItemInfo = styled(Paper)(({ theme }) => ({
    ...theme.typography.body1,
    textAlign: 'left',
    color: theme.palette.text.primary,
    fontFamily: 'monospace',
    padding: '10px',
}));

export default function UpdatedInfoDialog({ open, setOpen, navLocation, title, message }: Props) {
    const navigate = useNavigate();

    const handleClose = () => {
        setOpen(false);

        if (navLocation)
            navigate(navLocation);
    };

    return (
        <Dialog
            open={open}
            onClose={handleClose}
            aria-labelledby="alert-dialog-title"
            aria-describedby="alert-dialog-description"
            TransitionComponent={Transition} keepMounted
            scroll='paper'
        >
            <DialogTitle>
                {title}
            </DialogTitle>
            <DialogContent>
                {message}
            </DialogContent>
            <DialogActions>
                <Button onClick={handleClose} autoFocus>Noted</Button>
            </DialogActions>
        </Dialog>
    );
}
