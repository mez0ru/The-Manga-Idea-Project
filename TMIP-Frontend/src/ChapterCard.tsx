import { useState, useEffect, useRef } from 'react'
import { BASE_URL } from './api/axios';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Chapter } from './ChaptersPage';

interface Props {
    chapter: Chapter;
}

export default function SeriesCard({ chapter }: Props) {
    const axiosPrivate = useAxiosPrivate();
    const imageRef = useRef<HTMLImageElement>(null);


    useEffect(() => {
        let objectUrl: string
        const ev = () => URL.revokeObjectURL(objectUrl);

        const imageOnLoad = async () => {
            console.log('loading image')
            // Fetch the image.
            try {
                const response = await axiosPrivate.get(`${BASE_URL}/api/chapter/${chapter.id}/cover`, { responseType: 'blob' });

                // Create an object URL from the data.
                const blob = response.data;
                objectUrl = URL.createObjectURL(blob);

                // Update the source of the image.
                imageRef.current!!.src = objectUrl;
            } catch (err: unknown) {

            }
            imageRef.current?.addEventListener('load', ev);
        }
        imageOnLoad();

        return () => {
            imageRef.current?.removeEventListener('load', ev);
        }
    }, [])


    return (
        <Card sx={{ maxWidth: 150 }}>
            <CardActionArea>
                <CardMedia
                    component="img"
                    alt={chapter.name}
                    ref={imageRef}
                />
                <CardContent>
                    <Typography gutterBottom variant="body1">
                        {chapter.name}
                    </Typography>
                </CardContent>
            </CardActionArea>
        </Card>
    )
}
