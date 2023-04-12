import { AxiosError } from 'axios';
import { useRef, useState, useEffect } from 'react'
import { useParams } from 'react-router-dom';
import Grid from '@mui/material/Grid';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Image from './Components/Image';

export default function Viewer() {

    // chapter id
    let { seriesId, id } = useParams();

    // If true, display a single image only.
    // Only applicable to wide screens / two page configuration.
    const [isWide, setIsWide] = useState(false);
    const [currentId, setCurrentId] = useState(0);
    const [imageIds, setImageIds] = useState<number[]>([]);

    const axiosPrivate = useAxiosPrivate();

    useEffect(() => {
        const getImageIds = async () => {
            try {
                const response = await axiosPrivate.get(`/api/chapter/info/${id}`);
                setImageIds(response?.data as number[]);

            } catch (err) {
                if (err instanceof AxiosError) {
                    // if (!err?.response) {
                    //     setErrMsg('No Server Response');
                    // } else if (err.response?.status === 400) {
                    //     setErrMsg('Missing Email or Password');
                    // } else if (err.response?.status === 401) {
                    //     setErrMsg('Unauthorized');
                    // } else {
                    //     setErrMsg('Login Failed');
                    // }
                    // errRef.current?.focus();

                    console.log(err?.response);
                }
            }
        }

        getImageIds();

        return () => {
            setImageIds([]);
        }
    }, []);

    const forward = () => setCurrentId(x => x === 0 ? x + 1 : (isWide ? x + 1 : x + 2));
    const backward = () => setCurrentId(x => x === 0 ? x : (isWide ? x + 1 : x - 2));

    return (
        <Grid container rowSpacing={0}>
            {imageIds.length ? (isWide || currentId === 0) ? <Grid item>
                <Image chapterId={parseInt(id!!)} id={imageIds[currentId]} alt="whatever" setIsWide={setIsWide} onClick={forward} />
            </Grid> : <><Grid item xs={6}>
                <Image chapterId={parseInt(id!!)} id={imageIds[currentId]} alt="whatever" setIsWide={setIsWide} onClick={backward} />
            </Grid>
                <Grid item xs={6}>
                    <Image chapterId={parseInt(id!!)} id={imageIds[currentId + 1]} alt="whatever" setIsWide={setIsWide} onClick={forward} />
                </Grid></>
                : <></>}
        </Grid >
    )
}
