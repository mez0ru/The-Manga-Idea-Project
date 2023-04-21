import { forwardRef, useEffect, useState } from 'react'
import Button from '@mui/material/Button';
import Dialog from '@mui/material/Dialog';
import DialogActions from '@mui/material/DialogActions';
import DialogContent from '@mui/material/DialogContent';
import DialogContentText from '@mui/material/DialogContentText';
import DialogTitle from '@mui/material/DialogTitle';
import { TransitionProps } from '@mui/material/transitions';
import Zoom from '@mui/material/Zoom';
import useAxiosPrivate from '../hooks/useAxiosPrivate';
import { AxiosError } from 'axios';
import LinearProgress from '@mui/material/LinearProgress';
import Alert from '@mui/material/Alert';
import UpdatedInfoDialog, { CodeItemRed } from './UpdatedInfoDialog';

interface Props {
    open: boolean;
    setOpen: React.Dispatch<React.SetStateAction<boolean>>;
    id?: number;
    name: string;
}

const Transition = forwardRef(function Transition(
    props: TransitionProps & {
        children: React.ReactElement<any, any>;
    },
    ref: React.Ref<unknown>,
) {
    return <Zoom ref={ref} {...props} />;
});

export default function DeleteSeriesDialog({ open, setOpen, id, name }: Props) {
    const [isLoading, setIsLoading] = useState(false);
    const axiosPrivate = useAxiosPrivate();
    const [errMsg, setErrMsg] = useState('')
    const [showInfo, setShowInfo] = useState(false);

    const deleteSeries = async () => {
        if (id) {
            setIsLoading(true);
            try {
                const response = await axiosPrivate.delete(`/api/v2/series/${id}`);
                if (response.status === 204) {
                    console.log('no content 204');
                } else {
                    console.log('other ....');
                }

                console.log(response);
                handleClose();
                setShowInfo(true);
            } catch (err: unknown) {
                if (err instanceof AxiosError) {
                    if (err.response?.status === 423)
                        setErrMsg("Database is locked, can't delete.");
                }
                setIsLoading(false);
            }
        } else {
            setErrMsg("Id is not set.");
        }
    }


    const handleClose = () => {
        setIsLoading(false);
        setOpen(false);
    };

    return (
        <><Dialog
            open={open}
            onClose={handleClose}
            aria-labelledby="alert-dialog-title"
            aria-describedby="alert-dialog-description"
            TransitionComponent={Transition} keepMounted
        >
            {isLoading ? <LinearProgress variant='query' /> : null}
            <DialogTitle>
                {`Delete "${name}"`}
            </DialogTitle>
            <DialogContent>
                {errMsg ? <Alert severity="error">{errMsg}</Alert> : null}
                <DialogContentText>
                    You are about to <strong>Remove</strong> "{name}" series.<br />
                    Note that this action will not affect the actual files.<br />
                    Do you confirm this operation?
                </DialogContentText>
            </DialogContent>
            <DialogActions>
                <Button onClick={handleClose} autoFocus>Cancel</Button>
                <Button onClick={deleteSeries}>
                    Confirm
                </Button>
            </DialogActions>
        </Dialog>

            <UpdatedInfoDialog open={showInfo} setOpen={setShowInfo} navLocation='/home' title="Action Finished" message={
                <CodeItemRed elevation={3}>
                    Series "{name}" has been deleted.
                </CodeItemRed>
            } />
        </>
    );
}
